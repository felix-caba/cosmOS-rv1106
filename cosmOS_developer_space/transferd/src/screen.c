#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "../include/font.h"
#include "../include/transferd.h"

/*

    Controlador SSD1306 desarrollado por felixcaba@proton.me (CosmOS Project).

    Cada direccion de memoria controla 8 pixeles verticales en una columna.

    128 columnas (segmentos horizontales) = 8 páginas (direcciones de memoria)

    128x8 = 1024 direcciones de memoria.

    El ancho es 128 PIXELES (128 direcciones de memoria por página).
    El alto es 64 PIXELES (8 páginas, cada una 8 pixeles).

    Cada direccion controla 8 pixeles verticales. Cada página tiene 128 direcciones.

    Pagina 0: 128 direcciones de memoria. Cada dirección son 8 pixeles verticales,
    ya que cada pagina tiene 8 pixeles de alto.

    ┌─────────────── 128 columnas ─────────────┐
    │ ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●● │ ← Fila 0
    │ ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●● │ ← Fila 1
    │ ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●● │ ← Fila 2
    │ ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●● │ ← Fila 3
    │ ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●● │ ← Fila 4
    │ ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●● │ ← Fila 5
    │ ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●● │ ← Fila 6
    │ ●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●●● │ ← Fila 7
    └──────────────────────────────────────────┘ ← PAGINA 0

    128/16 = 8. 8 caracteres de largo por fila.
    pero de vertical, al ser tambien 16, son 2 filas ocupadas de vertical.

*/

#define SSD1306_I2C_ADDR 0x3C

#define SSD1306_PAGES 8
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT SSD1306_PAGES * 8

#define SSD1306_COMMAND 0x00
#define SSD1306_DATA 0x40

#define SSD1306_DISPLAY_OFF 0xAE
#define SSD1306_DISPLAY_ON 0xAF

#define SSD1306_SET_PAGE_ADDR 0x22
#define SSD1306_SET_COLUMN_ADDR 0x21
#define SSD1306_MEMORY_MODE 0x20

typedef struct
{
    int fd;                                        // file descriptor del i2c
    int reset_fd;                                  // sin uso ahora
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES]; // mapeo de la pantalla fisica.
    //
} ssd1306_t;

static ssd1306_t global_display;
static int screen_initialized = 0;

void delay_ms(int milliseconds)
{
    usleep(milliseconds * 1000);
}

int ssd1306_send_command(ssd1306_t *display, uint8_t cmd)
{
    uint8_t buffer[2] = {SSD1306_COMMAND, cmd};
    int result = write(display->fd, buffer, 2);
    if (result != 2)
    {
        perror("Error sending command");
        return -1;
    }
    return 0;
}

/*
    Configura el modo de direccionamiento de la pantalla SSD1306.
    El modo horizontal es el mas común
*/

int ssd1306_set_addressing_mode(ssd1306_t *display)
{
    if (ssd1306_send_command(display, SSD1306_MEMORY_MODE) < 0)
        return -1;
    if (ssd1306_send_command(display, 0x00) < 0)
        return -1; // Modo horizontal
    return 0;
}

int ssd1306_set_write_area(ssd1306_t *display, uint8_t col_start, uint8_t col_end,
                           uint8_t page_start, uint8_t page_end)
{

    if (ssd1306_send_command(display, SSD1306_SET_COLUMN_ADDR) < 0)
        return -1; // Columnas
    if (ssd1306_send_command(display, col_start) < 0)
        return -1;
    if (ssd1306_send_command(display, col_end) < 0)
        return -1;

    if (ssd1306_send_command(display, SSD1306_SET_PAGE_ADDR) < 0)
        return -1; // Paginas
    if (ssd1306_send_command(display, page_start) < 0)
        return -1;
    if (ssd1306_send_command(display, page_end) < 0)
        return -1;

    return 0;
}

int ssd1306_init(ssd1306_t *display, const char *i2c_device)
{
    display->fd = open(i2c_device, O_RDWR);
    if (display->fd < 0)
    {
        perror("Error opening I2C device");
        return -1;
    }

    if (ioctl(display->fd, I2C_SLAVE, SSD1306_I2C_ADDR) < 0)
    {
        perror("Error setting I2C slave address");
        close(display->fd);
        return -1;
    }

    // 1. Display OFF
    if (ssd1306_send_command(display, 0xAE) < 0)
        return -1;

    // 2. Set display clock divide ratio/oscillator frequency
    if (ssd1306_send_command(display, 0xD5) < 0)
        return -1;
    if (ssd1306_send_command(display, 0x80) < 0)
        return -1;

    // 3. Set multiplex ratio
    if (ssd1306_send_command(display, 0xA8) < 0)
        return -1;
    if (ssd1306_send_command(display, 0x3F) < 0)
        return -1; // 64MUX

    // 4. Set display offset
    if (ssd1306_send_command(display, 0xD3) < 0)
        return -1;
    if (ssd1306_send_command(display, 0x00) < 0)
        return -1;

    // 5. Set start line address
    if (ssd1306_send_command(display, 0x40) < 0)
        return -1;

    // 6. Charge pump setting
    if (ssd1306_send_command(display, 0x8D) < 0)
        return -1;
    if (ssd1306_send_command(display, 0x14) < 0)
        return -1; // Enable charge pump

    // 7. Memory addressing mode
    if (ssd1306_send_command(display, 0x20) < 0)
        return -1;
    if (ssd1306_send_command(display, 0x00) < 0)
        return -1; // Horizontal mode

    // 8. Set segment re-map
    if (ssd1306_send_command(display, 0xA1) < 0)
        return -1;

    // 9. Set COM output scan direction
    if (ssd1306_send_command(display, 0xC8) < 0)
        return -1;

    // 10. Set COM pins hardware configuration
    if (ssd1306_send_command(display, 0xDA) < 0)
        return -1;
    if (ssd1306_send_command(display, 0x12) < 0)
        return -1;

    // 11. Set contrast control
    if (ssd1306_send_command(display, 0x81) < 0)
        return -1;
    if (ssd1306_send_command(display, 0xCF) < 0)
        return -1;

    // 12. Set pre-charge period
    if (ssd1306_send_command(display, 0xD9) < 0)
        return -1;
    if (ssd1306_send_command(display, 0xF1) < 0)
        return -1;

    // 13. Set VCOMH deselect level
    if (ssd1306_send_command(display, 0xDB) < 0)
        return -1;
    if (ssd1306_send_command(display, 0x40) < 0)
        return -1;

    // 14. Entire display ON (resume to RAM content)
    if (ssd1306_send_command(display, 0xA4) < 0)
        return -1;

    // 15. Set normal display
    if (ssd1306_send_command(display, 0xA6) < 0)
        return -1;

    // 16. Display ON
    if (ssd1306_send_command(display, 0xAF) < 0)
        return -1;

    delay_ms(100);

    memset(display->buffer, 0, sizeof(display->buffer));

    return 0;
}

const uint8_t *get_char_data_16x16(char c)
{
    const uint8_t *font_data = font16x16;
    uint8_t width = font_data[2];
    uint8_t height = font_data[3];
    uint8_t first_char = font_data[4];
    uint8_t char_count = font_data[5];

    if (c < first_char || c >= (first_char + char_count))
    {
        c = ' ';
    }

    int char_index = c - first_char;
    int bytes_per_char = (width * height) / 8;      // 16x16 = 32 bytes por carácter
    int offset = 6 + (char_index * bytes_per_char); // 6 bytes de header + offset del carácter

    return &font_data[offset];
}

int ssd1306_send_data(ssd1306_t *display, uint8_t *data, int len)
{
    const int chunk_size = 32; // Enviar maximo de 32 bytes por que si no problemillas con el puto i2c

    for (int offset = 0; offset < len; offset += chunk_size)
    {
        int current_chunk = (len - offset > chunk_size) ? chunk_size : (len - offset);

        uint8_t buffer[current_chunk + 1];
        buffer[0] = SSD1306_DATA;
        memcpy(&buffer[1], &data[offset], current_chunk);

        int result = write(display->fd, buffer, current_chunk + 1);
        if (result != current_chunk + 1)
        {
            perror("Error sending data chunk");
            return -1;
        }

        // Pausa entre chunkazos
        usleep(1000); // 1ms
    }

    return 0;
}

int ssd1306_write_char_16x16(ssd1306_t *display, char c, uint8_t x, uint8_t page)
{
    if (x + 16 > SSD1306_WIDTH || page + 1 >= SSD1306_PAGES)
        return -1;

    const uint8_t *char_data = get_char_data_16x16(c);

    for (int p = 0; p < 2; p++)
    {
        if (ssd1306_set_write_area(display, x, x + 15, page + p, page + p) < 0)
            return -1;

        uint8_t page_data[16];

        for (int col = 0; col < 16; col++)
        {
            uint8_t column_byte = 0;

            for (int bit = 0; bit < 8; bit++)
            {
                int row = (p << 3) + bit; // p*8
                int i = row << 1;         // row*2
                uint16_t row_data = (char_data[i] << 8) | char_data[i + 1];

                column_byte |= ((row_data >> (15 - col)) & 1) << bit;
            }

            page_data[col] = column_byte;
        }

        if (ssd1306_send_data(display, page_data, 16) < 0)
            return -1;
    }

    return 0;
}

int ssd1306_write_string_16x16(ssd1306_t *display, const char *str, uint8_t x, uint8_t page)
{

    log_message("DEBUG: ssd1306_write_string_16x16 called with: %s", SCREEN_LOG_FILE, str);

    uint8_t pos_x = x;

    for (int i = 0; str[i] != '\0'; i++)
    {
        if (pos_x + 16 > SSD1306_WIDTH)
            break; // No hay espacio para otro carácter

        if (ssd1306_write_char_16x16(display, str[i], pos_x, page) < 0)
            return -1;
        pos_x += 16; // Siguiente carácter (16 pixeles de ancho)
    }
    return 0;
}

/*
    Los envia por páginas, cada página tiene 128 bytes (8 pixeles de alto).
    Para limpiar, es por que el I2C no soporta enviar 1024 bytes de una sola vez.
*/
int ssd1306_clear(ssd1306_t *display)
{
    uint8_t zeros[128];
    memset(zeros, 0, 128);

    for (int page = 0; page < 8; page++)
    {
        // Configurar área para cada página individualmente
        if (ssd1306_set_write_area(display, 0, 127, page, page) < 0)
            return -1;

        // Enviar datos de la página
        if (ssd1306_send_data(display, zeros, 128) < 0)
            return -1;
    }

    return 0;
}

void ssd1306_cleanup(ssd1306_t *display)
{
    if (display->fd >= 0)
    {
        ssd1306_send_command(display, SSD1306_DISPLAY_OFF);
        close(display->fd);
        display->fd = -1;
    }
}

/*
    Actualiza la pantalla con el nombre del objeto.
*/
/*
    Actualiza la pantalla parseando detecciones separadas por el ':'.
    Solo las que cambian
    Cada detección ocupa 2 pag
    Máximo 4
*/
void update_screen(const char *object_name)
{
    log_message("DEBUG: update_screen called with: %s", SCREEN_LOG_FILE, object_name);

    if (current_config.source_type == SOURCE_TYPE_I2C_TEMP)
    {
        log_message("Going to write string 16x16 to screen: %s", SCREEN_LOG_FILE, object_name);
        ssd1306_write_string_16x16(&global_display, object_name, 0, 2);
        log_message("Screen updated with: %s", SCREEN_LOG_FILE, object_name);
        return;
    }

    static char cached_detections[4][8] = {"", "", "", ""}; // cache detecciones
    static int cached_count = 0;

    if (!screen_initialized)
    {
        log_message("[WARNING] Screen not initialized, ignoring update", SCREEN_LOG_FILE);
        return;
    }

    // Clear y cache
    if (strcmp(object_name, "CLEAR") == 0)
    {
        ssd1306_clear(&global_display);

        for (int i = 0; i < 4; i++)
        {
            cached_detections[i][0] = '\0';
        }
        cached_count = 0;

        log_message("Screen cleared due to CLEAR signal", SCREEN_LOG_FILE);
        return;
    }

    // Parsear detecciones nuevas
    char new_detections[4][8];
    int new_count = 0;

    char temp_string[256];
    strncpy(temp_string, object_name, sizeof(temp_string) - 1);
    temp_string[sizeof(temp_string) - 1] = '\0';

    char *token = strtok(temp_string, ":");
    while (token != NULL && new_count < 4)
    {
        while (*token == ' ')
            token++;

        strncpy(new_detections[new_count], token, 7);
        new_detections[new_count][7] = '\0';

        int len = strlen(new_detections[new_count]);
        while (len > 0 && new_detections[new_count][len - 1] == ' ')
        {
            new_detections[new_count][--len] = '\0';
        }

        if (strlen(new_detections[new_count]) > 0)
        {
            new_count++;
        }

        token = strtok(NULL, ":");
    }
    bool screen_changed = false;

    // verificar si el número de detecciones ha cambiado
    if (new_count != cached_count)
    {
        screen_changed = true;
    }

    // comparar cada
    for (int i = 0; i < 4; i++)
    {
        char *new_det = (i < new_count) ? new_detections[i] : "";
        char *cached_det = cached_detections[i];

        if (strcmp(new_det, cached_det) != 0)
        {

            uint8_t page_start = i * 2;

            if (page_start + 1 < SSD1306_PAGES)
            {

                uint8_t zeros[128];
                memset(zeros, 0, 128);

                for (int p = 0; p < 2; p++)
                { // esto por que cada ocupa 2 paginotes
                    if (ssd1306_set_write_area(&global_display, 0, 127, page_start + p, page_start + p) >= 0)
                    {
                        ssd1306_send_data(&global_display, zeros, 128);
                    }
                }

                // escribir nueva.
                if (strlen(new_det) > 0)
                {
                    if (ssd1306_write_string_16x16(&global_display, new_det, 0, page_start) < 0)
                    {
                        log_message("[ERROR] Failed to write detection %d to screen: %s", SCREEN_LOG_FILE, i, new_det);
                    }
                    else
                    {
                        log_message("Updated page %d with: %s", SCREEN_LOG_FILE, i, new_det);
                    }
                }
            }

            strncpy(cached_detections[i], new_det, 7);
            cached_detections[i][7] = '\0';
            screen_changed = true;
        }
    }

    cached_count = new_count;

    if (screen_changed)
    {
        log_message("Screen updated partially with %d detections: %s", SCREEN_LOG_FILE, new_count, object_name);
    }
    else
    {
        log_message("Screen content unchanged, no refresh needed: %s", SCREEN_LOG_FILE, object_name);
    }
}

int start_screen(void)
{
    if (ssd1306_init(&global_display, "/dev/i2c-1") == 0)
    {
        screen_initialized = 1;
        log_message("Pantalla SSD1306 inicializada correctamente en i2c-1", SCREEN_LOG_FILE);

        // Mensaje inicial
        ssd1306_clear(&global_display);
        ssd1306_write_string_16x16(&global_display, "COSMOS", 0, 0);

        return 0;
    }
    else
    {
        screen_initialized = 0;
        log_message("Error al inicializar la pantalla SSD1306", SCREEN_LOG_FILE);
        return -1;
    }
}

void stop_screen(void)
{
    if (screen_initialized)
    {
        ssd1306_clear(&global_display);
        ssd1306_write_string_16x16(&global_display, "Stop", 0, 0);
        sleep(1);
        ssd1306_cleanup(&global_display);
        screen_initialized = 0;
        log_message("Pantalla SSD1306 detenida", SCREEN_LOG_FILE);
    }
}