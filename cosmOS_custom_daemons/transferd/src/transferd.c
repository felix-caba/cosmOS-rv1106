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
volatile sig_atomic_t terminate_ = 0;

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

void signal_handler(int sig) {
    if (sig == SIGTERM) {
        terminate_ = 1;
    }
}

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

void debug() {
    printf("Monitoring log file in real-time. Press Ctrl+C to exit.\n");

    FILE *log_file = fopen(LOG_FILE, "r");
    if (log_file == NULL) {
        perror("fopen (log file)");
        return;
    }

    fseek(log_file, 0, SEEK_END);

    while (1) {
        char line[256];
        while (fgets(line, sizeof(line), log_file) != NULL) {
            printf("%s", line);
            fflush(stdout);
        }
        usleep(100000);
    }
    fclose(log_file);
}

void version() {
    printf("Transfer Daemon Tool 1.0\n");
    printf("Copyright (C) 2025 felix@cosmOS\n");
    printf("License: GPLv3\n");
}

void start_daemon() {
    printf("Starting transfer daemon...\n");

    pid_t pid, sid; // Get the process ID and session ID
    pid = fork(); // Fork the process

    if (pid > 0) {
        printf("Daemon started with PID: %d\n", pid);
        exit(EXIT_SUCCESS);
    }

    sid = setsid();

    if (chdir("/") < 0) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO); 
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    umask(0);
    signal(SIGTERM, signal_handler);

    FILE *fp = fopen(PID_FILE, "w");
    if (fp == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "%d\n", getpid());
    fclose(fp);

    while (!terminate_) {
        log_message("Daemon is running...");
        sleep(5);
    }

}

void stop_daemon() {
    log_message("Stopping transfer daemon...");

    FILE *fp = fopen(PID_FILE, "r");
    if (fp == NULL) {
        log_message("Could not open PID file %s. Daemon may not be running.", PID_FILE);
        return;
    }

    pid_t pid;
    if (fscanf(fp, "%d", &pid) != 1) {
        log_message("Could not read PID from file %s. File may be corrupted.", PID_FILE);
        fclose(fp);
        return;
    }
    fclose(fp);

    if (kill(pid, SIGTERM) == -1) {
        log_message("Could not send SIGTERM to process %d. Process may not exist or you may lack permissions.", pid);
    } else {
        log_message("SIGTERM sent to process %d.", pid);
    }
}

int main(int argc, char *argv[]) {
    if (strcmp(argv[1], "start") == 0) {
        start_daemon();
    } else if (strcmp(argv[1], "stop") == 0) {
        stop_daemon();
    } else if (strcmp(argv[1], "status") == 0) {
        printf("Transfer daemon is running.\n");
    } else if (strcmp(argv[1], "-h") == 0) {
        usage();
    } else if (strcmp(argv[1], "-v") == 0) {
        version();
    } else if (strcmp(argv[1], "-d") == 0) {
        debug();
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