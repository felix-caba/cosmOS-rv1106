#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h> 
#include <errno.h> 

#include "../include/transferd.h"

volatile sig_atomic_t terminate_ = 0;

void signal_handler(int sig) {
    if (sig == SIGTERM) {
        terminate_ = 1;
    }
}

int start_daemon() {
    
    system("echo 3 > /proc/sys/kernel/printk 2>/dev/null");

    log_message("Attempting to start transfer daemon...", LOG_FILE);

    FILE *pid_file_check = fopen(PID_FILE, "r");
    if (pid_file_check != NULL) {
        pid_t existing_pid;
        if (fscanf(pid_file_check, "%d", &existing_pid) == 1) {
            // Verificar si el proceso con existing_pid realmente está corriendo
            if (kill(existing_pid, 0) == 0) {
                log_message("Daemon is already running with PID: %d", LOG_FILE, existing_pid);
                fclose(pid_file_check);
                return -1; // Indicar que ya está corriendo
            } else if (errno == ESRCH) {
                log_message("Found stale PID file for PID %d, process not running. Overwriting.", LOG_FILE, existing_pid);
                // El proceso no existe, el archivo PID es obsoleto, podemos continuar.
            } else {
                log_message("Error checking status of existing PID %d: %s. Aborting.", LOG_FILE, existing_pid, strerror(errno));
                fclose(pid_file_check);
                return -1;
            }
        }
        fclose(pid_file_check); 
    }

    pid_t pid, sid;
    pid = fork();

    if (pid < 0) { // Error en fork
        log_message("[ERROR] Fork failed: %s", LOG_FILE, strerror(errno));
        exit(EXIT_FAILURE); // Salir en el proceso original si fork falla
    }

    if (pid > 0) { // Proceso padre
        log_message("Daemon process created with PID: %d", LOG_FILE, pid);
        exit(EXIT_SUCCESS); // El padre termina exitosamente
    }
    umask(0); 

    sid = setsid();
    if (sid < 0) {
        log_message("setsid failed: %s", LOG_FILE, strerror(errno));
        return -1;
    }

    if (chdir("/") < 0) {
        log_message("chdir to / failed: %s", LOG_FILE, strerror(errno));
        return -1;
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int fd_dev_null = open("/dev/null", O_RDWR);
    if (fd_dev_null != -1) {
        dup2(fd_dev_null, STDIN_FILENO);
        dup2(fd_dev_null, STDOUT_FILENO);
        dup2(fd_dev_null, STDERR_FILENO);
        if (fd_dev_null > 2) { 
            close(fd_dev_null);
        }
    }

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler); 

    FILE *pid_file_write = fopen(PID_FILE, "w");

    if (pid_file_write == NULL) {
        log_message("[ERROR] Failed to open PID file %s for writing: %s", PID_FILE, strerror(errno));
        _exit(EXIT_FAILURE);
    }

    fprintf(pid_file_write, "%d\n", getpid());
    fclose(pid_file_write);

    log_message("Daemon (PID: %d) started successfully and is running.", LOG_FILE, getpid());

    if (transferd_logic_init() != 0) {
        log_message("[ERROR] Transferd: Failed to initialize transfer logic. Daemon exiting.", LOG_FILE);
        unlink(PID_FILE);
        _exit(EXIT_FAILURE);
    }

    while (!terminate_) {
        transferd_loop();
        usleep(200000);
    }

    if (current_config.output_type == OUTPUT_TYPE_SCREEN) {
        stop_screen();
    }
    if (current_config.output_type == OUTPUT_TYPE_HTTP) {
        stop_http_server();
    }

    log_message("Transferd (PID: %d) received termination signal. Shutting down...", LOG_FILE , getpid());
    if (unlink(PID_FILE) != 0) {
        log_message("[ERROR] Failed to remove PID file %s: %s", PID_FILE, strerror(errno));
    }
    log_message("Transferd (PID: %d) has shut down.", LOG_FILE, getpid());
    return 0;
}

int stop_daemon() {
    log_message("Attempting to stop transfer daemon...", LOG_FILE);

    FILE *pid_file = fopen(PID_FILE, "r");
    if (pid_file == NULL) {
        log_message("[ERROR] PID file %s not found. Daemon may not be running or was not started correctly.", LOG_FILE, PID_FILE);
        return -1;
    }

    pid_t pid_to_kill;
    if (fscanf(pid_file, "%d", &pid_to_kill) != 1) {
        log_message("[ERROR] Could not read PID from file %s. File may be corrupted.", LOG_FILE, PID_FILE);
        fclose(pid_file);
        return -1; 
    }
    fclose(pid_file);

    log_message("Sending SIGTERM to process %d...", LOG_FILE, pid_to_kill);
    if (kill(pid_to_kill, SIGTERM) == -1) {
        if (errno == ESRCH) {
            log_message("[ERROR] Process with PID %d not found. Daemon may have already stopped or PID file is stale.", LOG_FILE, pid_to_kill);
            unlink(PID_FILE);
        } else {
            log_message("[ERROR] Could not send SIGTERM to process %d: %s.", LOG_FILE, pid_to_kill, strerror(errno));
        }
        return -1;
    }

    log_message("SIGTERM sent to process %d. Waiting for daemon to shut down...", LOG_FILE, pid_to_kill);
    return 0; // Señal enviada exitosamente
}