#ifndef TRANSFERD_H
#define TRANSFERD_H

#include <stdint.h>

#define VERSION "1.0"
#define AUTHOR "felix@cosmOS"
#define LICENSE "GPLv3"

#define DAEMON_NAME "transferd"
#define PID_FILE "/var/run/transferd.pid"
#define LOG_FILE "/var/log/transferd.log"
#define CONFIG_FILE "/etc/transferd.conf"

typedef enum {
    SOURCE_TYPE_NONE,
    SOURCE_TYPE_GPIO,
    SOURCE_TYPE_YOLO
} source_type_t;

typedef enum {
    OUTPUT_TYPE_SCREEN,
    OUTPUT_TYPE_HTTP
} output_type_t;

typedef struct {
    source_type_t source_type;
    output_type_t output_type;
    
    union {
        struct {
            int pin;
            int poll_interval_ms;
        } gpio;
    } source_config;

} transferd_config_t;

int start_daemon(void);
int stop_daemon(void);
int debug_daemon(void);

void log_message(const char* format, ...);
void log_error(const char* format, ...);

int transferd_loop(void);
int transferd_logic_init(void);

int transferd_load_config(const transferd_config_t* config);
int load_config_from_file(transferd_config_t* config_out);
int save_config_to_file(const transferd_config_t* config_in);
const char* source_type_to_string(source_type_t type);

int start_http_server(void);
void stop_http_server(void);
void update_detection(const char* object_name);

#endif /* TRANSFERD_H */