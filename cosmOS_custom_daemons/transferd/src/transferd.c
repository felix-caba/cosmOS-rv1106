#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>

#include "../include/transferd.h"



#define VERSION "1.0"
#define AUTHOR "felix@cosmOS"
#define LICENSE "GPLv3"
#define DAEMON_NAME "transferd"
#define PID_FILE "/var/run/transferd.pid"

/*Transfer daemon*/
typedef enum {
    TRANSFER_TYPE_UART,
    TRANSFER_TYPE_SOCKET
} transfer_type_t;

struct transferd {
    transfer_type_t type;
    char *device;
    int baudrate;
    int socket_port;
};

void usage() {
    printf("Usage for Transfer Daemon Tool 1.0: ");
    printf("transferd <command> [options]\n");
    printf("Commands:\n");
    printf("  start   Start the transfer daemon\n");
    printf("  stop    Stop the transfer daemon\n");
    printf("  status  Show the status of the transfer daemon\n");
    printf("Options:\n");
    printf("  -t + <type>,   Specify the transfer daemon type (UART, SOCKET)\n");
    printf("  -h,   Show this help message\n");
    printf("  -v,   Show version information\n");
    printf("  -d,   Show debugging information\n");
}

int debug_daemon() {
    printf("Monitoring log file in real-time. Press Ctrl+C to exit.\n");

    FILE *log_file = fopen(LOG_FILE, "r");
    if (log_file == NULL) {
        perror("fopen (log file)");
        return -1;
    }
 
    if (fseek(log_file, 0, SEEK_END) != 0) {
        perror("fseek (log file)");
        fclose(log_file);
        return -1;
    }


     while (1) {
        char line[256];
        // Leer todas las líneas nuevas disponibles
        while (fgets(line, sizeof(line), log_file) != NULL) {
            printf("%s", line);
            fflush(stdout); // Asegurar que la salida se muestre inmediatamente
        }

        // IMPORTANTE: Limpiar los indicadores de EOF y error del stream
        // para que fgets pueda detectar nuevas escrituras en el archivo.
        clearerr(log_file);

        usleep(200000);
    }

    fclose(log_file);
    return 0;
}

void version() {
    printf("Transfer Daemon Tool 1.0\n");
    printf("Copyright (C) 2025 felix@cosmOS\n");
    printf("License: GPLv3\n");
}

int main(int argc, char *argv[]) {
    if (strcmp(argv[1], "start") == 0) {
        start_daemon();
    } else if (strcmp(argv[1], "stop") == 0) {
        stop_daemon();
    } else if (strcmp(argv[1], "status") == 0) {

        FILE *fp = fopen(PID_FILE, "r");
        if (fp == NULL) {
            printf("Daemon is not running (PID file not found).\n");
        } else {
            pid_t pid;
            if (fscanf(fp, "%d", &pid) != 1) {
                printf("Daemon is not running (PID file corrupted).\n");
            } else {
                printf("Daemon is running with PID: %d\n", pid);
            }
            fclose(fp);
        }
    } else if (strcmp(argv[1], "-h") == 0) {
        usage();
    } else if (strcmp(argv[1], "-v") == 0) {
        version();
    } else if (strcmp(argv[1], "-d") == 0) {
        debug_daemon();
    } else if (strcmp(argv[1], "-t") == 0) {
        if (strcmp(argv[2], "UART") == 0) {
            printf("Transfer type set to UART.\n");
        } else if (strcmp(argv[2], "SOCKET") == 0) {
            printf("Transfer type set to SOCKET.\n");
        } else {
            printf("Invalid transfer type. Use 'UART' or 'SOCKET'.\n");
        }
    } else {
        usage();
        return 1;
    }
  return 0;
}