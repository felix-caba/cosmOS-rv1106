#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <stdio.h>
#include <font.h>

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
*/

#define SSD1306_I2C_ADDR    0x3C

#define SSD1306_PAGES       8
#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      SSD1306_PAGES * 8

#define SSD1306_COMMAND     0x00
#define SSD1306_DATA        0x40

#define SSD1306_DISPLAY_OFF 0xAE
#define SSD1306_DISPLAY_ON  0xAF

#define SSD1306_SET_PAGE_ADDR    0x22
#define SSD1306_SET_COLUMN_ADDR  0x21
#define SSD1306_MEMORY_MODE      0x20

static ssd1306_t global_display;
static int screen_initialized = 0;

typedef struct {
    int fd;
    int reset_fd;
    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES]; 
} ssd1306_t;

void delay_ms(int milliseconds) {
    usleep(milliseconds * 1000);
}

int ssd1306_send_command(ssd1306_t *display, uint8_t cmd) {
    uint8_t buffer[2] = {SSD1306_COMMAND, cmd};
    int result = write(display->fd, buffer, 2);
    if (result != 2) {
        perror("Error sending command");
        return -1;
    }
    return 0;
}

int ssd1306_init(ssd1306_t *display, const char *i2c_device) {
    
    display->fd = open(i2c_device, O_RDWR);     // Abrir dispositivo i2c lectura/escritura (pipe)
    if (display->fd < 0) {
        perror("Error opening I2C device");     // No se asigna el i2c
        return -1;
    }
    
    if (ioctl(display->fd, I2C_SLAVE, SSD1306_I2C_ADDR) < 0) {      // Direccion esclavo i2c
        perror("Error setting I2C slave address");
        close(display->fd);     // Cierra el pipe del dispositivo i2c
        return -1;
    }
    
    delay_ms(10);       // Datasheet retraso
    
    if (ssd1306_send_command(display, SSD1306_DISPLAY_OFF) < 0) {   // Display off
        close(display->fd);
        return -1;
    }

    if (ssd1306_set_addressing_mode(display) < 0) {     // Configurar modo de direccionamiento 
        close(display->fd);
        return -1;
    }
    
    delay_ms(10);
    
    if (ssd1306_send_command(display, SSD1306_DISPLAY_ON) < 0) {    // Display on
        close(display->fd);
        return -1;
    }
    
    delay_ms(100);
    
    memset(display->buffer, 0, sizeof(display->buffer));        // Limpiar buffer de pantalla
    
    return 0;
}


int ssd1306_send_data(ssd1306_t *display, uint8_t *data, int len) {
    uint8_t buffer[len + 1];
    buffer[0] = SSD1306_DATA;  // Control byte para datos
    memcpy(&buffer[1], data, len);   // Copiar datos al buffer
    
    int result = write(display->fd, buffer, len + 1);   // Enviar datos al dispositivo I2C
    if (result != len + 1) {
        perror("Error sending data");
        return -1;
    }
    return 0;
}

/*
    Configura el modo de direccionamiento de la pantalla SSD1306.
    El modo horizontal es el mas común
*/

int ssd1306_set_addressing_mode(ssd1306_t *display) {
    if (ssd1306_send_command(display, SSD1306_MEMORY_MODE) < 0) return -1;
    if (ssd1306_send_command(display, 0x00) < 0) return -1;  // Modo horizontal
    return 0;
}

int ssd1306_set_write_area(ssd1306_t *display, uint8_t col_start, uint8_t col_end, 
                           uint8_t page_start, uint8_t page_end) {
   
    if (ssd1306_send_command(display, SSD1306_SET_COLUMN_ADDR) < 0) return -1;      // Columnas
    if (ssd1306_send_command(display, col_start) < 0) return -1;
    if (ssd1306_send_command(display, col_end) < 0) return -1;
    
 
    if (ssd1306_send_command(display, SSD1306_SET_PAGE_ADDR) < 0) return -1;        // Paginas
    if (ssd1306_send_command(display, page_start) < 0) return -1;
    if (ssd1306_send_command(display, page_end) < 0) return -1;
    
    return 0;
}

int ssd1306_write_char(ssd1306_t *display, char c, uint8_t x, uint8_t page) {
    if (x >= SSD1306_WIDTH || page >= SSD1306_PAGES) return -1;
    
    const uint8_t *char_bitmap; // Bitmap desde 8x8.h
    
    if (c >= 0 && c < 128) {       // Rango válido
        char_bitmap = (uint8_t*)font8x8_basic[(int)c];
    } else {
        char_bitmap = (uint8_t*)font8x8_basic[32];  // No está en el rango, usar espacio en blanco
    }
    
    // Configurar área: 8 columnas x 1 página
    if (ssd1306_set_write_area(display, x, x + 7, page, page) < 0) return -1;
    
    // Enviar datos del carácter real desde la fuente
    return ssd1306_send_data(display, (uint8_t*)char_bitmap, 8);
}

int ssd1306_write_string(ssd1306_t *display, const char *str, uint8_t x, uint8_t page) {
    uint8_t pos_x = x;
    
    for (int i = 0; str[i] != '\0'; i++) {
        if (pos_x + 8 > SSD1306_WIDTH) break;  // End
        
        if (ssd1306_write_char(display, str[i], pos_x, page) < 0) return -1;
        pos_x += 8;  // Siguiente carácter (8 pixeles de ancho)
    }
    return 0;
}

int ssd1306_clear(ssd1306_t *display) {
    if (ssd1306_set_write_area(display, 0, 127, 0, 7) < 0) return -1;
    
    uint8_t zeros[128];    // Enviar 1024 bytes de ceros (toda la memoria)
    memset(zeros, 0, 128);
    
    for (int page = 0; page < 8; page++) {
        if (ssd1306_send_data(display, zeros, 128) < 0) return -1;
    }
    return 0;
}


void ssd1306_cleanup(ssd1306_t *display) {
    if (display->fd >= 0) {
        ssd1306_send_command(display, SSD1306_DISPLAY_OFF);
        close(display->fd);
        display->fd = -1;
    }
}

/*
    Actualiza la pantalla con el nombre del objeto.
*/
void update_screen(const char *object_name) {
    if (!screen_initialized) {
        log_message("[WARNING] Screen not initialized, ignoring update", SCREEN_LOG_FILE);
        return;
    }
    
    ssd1306_clear(&global_display);
    
    ssd1306_write_string(&global_display, "COSMOS Detection:", 0, 0);   // Esto en la página 0
    
    char display_text[16];
    strncpy(display_text, object_name, 15);
    display_text[15] = '\0';
    ssd1306_write_string(&global_display, display_text, 0, 1);
    
    log_message("Screen updated with detection: %s", SCREEN_LOG_FILE, object_name);
}

int start_screen(void) {
    if (ssd1306_init(&global_display, "/dev/i2c-1") == 0) {
        screen_initialized = 1;
        log_message("Pantalla SSD1306 inicializada correctamente en i2c-1", SCREEN_LOG_FILE);
        
        // Mensaje inicial
        ssd1306_clear(&global_display);
        ssd1306_write_string(&global_display, "COSMOS Ready", 0, 0);
        ssd1306_write_string(&global_display, "Waiting...", 0, 1);
        
        return 0;
    } else {
        screen_initialized = 0;
        log_message("Error al inicializar la pantalla SSD1306", SCREEN_LOG_FILE);
        return -1;
    }
}

void stop_screen(void) {
    if (screen_initialized) {
        ssd1306_clear(&global_display);
        ssd1306_write_string(&global_display, "COSMOS Stopped", 0, 0);
        sleep(1);  
        ssd1306_cleanup(&global_display);
        screen_initialized = 0;
        log_message("Pantalla SSD1306 detenida", SCREEN_LOG_FILE);
    }
}

int main() {
    ssd1306_t display;
    
    if (ssd1306_init(&display, "/dev/i2c-1") == 0) {
        log_message("Display inicializado correctamente en i2c-1", SCREEN_LOG_FILE);

        // Configurar modo de direccionamiento
        if (ssd1306_set_addressing_mode(&display) < 0) {
            log_message("Error configurando addressing mode", SCREEN_LOG_FILE);
            ssd1306_cleanup(&display);
            return -1;
        }
        
        ssd1306_clear(&display);
        
        // Probar diferentes caracteres reales
        ssd1306_write_string(&display, "COSMOS", 0, 0);        // Página 0
        ssd1306_write_string(&display, "Hello World!", 0, 1);  // Página 1  
        ssd1306_write_string(&display, "123 ABC xyz", 0, 2);   // Página 2
        ssd1306_write_string(&display, "!@#$%^&*()", 0, 3);    // Página 3
        
        sleep(10);  // Mostrar durante 10 segundos
        ssd1306_cleanup(&display);
    } else {
        log_message("Error inicializando display", SCREEN_LOG_FILE);
        return -1;
    }
    
    return 0;
}