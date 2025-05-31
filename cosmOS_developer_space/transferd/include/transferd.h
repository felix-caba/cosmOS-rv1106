#ifndef TRANSFERD_H
#define TRANSFERD_H

#include <stdint.h>

#define VERSION "1.0"
#define AUTHOR "felix@cosmOS"
#define LICENSE "GPLv3"

#define DAEMON_NAME "transferd"
#define PID_FILE "/var/run/transferd.pid"
#define LOG_FILE "/var/log/transferd.log"
#define YOLO_LOG_FILE "/tmp/yolo.log"
#define API_LOG_FILE "/tmp/api.log"
#define SCREEN_LOG_FILE "/tmp/screen.log"
#define CONFIG_FILE "/etc/transferd.conf"



typedef enum {
    SOURCE_TYPE_NONE,
    SOURCE_TYPE_I2C_TEMP,
    SOURCE_TYPE_YOLO
} source_type_t;

typedef enum {
    OUTPUT_TYPE_SCREEN,
    OUTPUT_TYPE_HTTP
} output_type_t;

typedef struct {
    source_type_t source_type;
    output_type_t output_type;


} transferd_config_t;

extern transferd_config_t current_config;   // Configuracion global

int start_daemon(void);
int stop_daemon(void);
int debug_daemon(void);

void log_message(const char* format,  char *log_file, ...);


int transferd_loop(void);
int transferd_logic_init(void);

int transferd_load_config(const transferd_config_t* config);
int load_config_from_file(transferd_config_t* config_out);
int save_config_to_file(const transferd_config_t* config_in);
const char* source_type_to_string(source_type_t type);

int start_http_server(void);
void stop_http_server(void);
void update_api(const char* object_name);

int start_screen(void);
void stop_screen(void);
void update_screen(const char* object_name);


#endif /* TRANSFERD_H */