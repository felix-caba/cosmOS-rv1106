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
    TARGET_TYPE_NONE,
    TARGET_TYPE_UART,
    TARGET_TYPE_SOCKET
} target_type_t;

typedef struct {
    source_type_t source_type;
    target_type_t target_type;

    union {
        struct {
            int pin;
            int poll_interval_ms;
        } gpio;

        struct {
            char* model_path;
            int threshold;
        } yolo;
    } source_config;

    union {
        struct {
            char* device;
            int baudrate;
        } uart;

        struct {
            char* host;
            int port;
            int use_ssl;
        } socket;
    } target_config;

    int verbose;
} transferd_config_t;

int transferd_init(transferd_config_t* config);
void transferd_cleanup(void);
int transferd_process_data(void);

int start_daemon(transferd_config_t* config);
int stop_daemon(void);
int status_daemon(void);

void log_message(const char* format, ...);
void log_error(const char* format, ...);
void log_debug(const char* format, ...);

#endif /* TRANSFERD_H */