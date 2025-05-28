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
#include <sys/wait.h>
#include <errno.h>


transferd_config_t current_config;
static int yolo_pipe_read_fd = -1;
static pid_t yolo_pid = -1;

void usage()
{
    printf("Usage for Transfer Daemon Tool 1.0: ");
    printf("transferd <command> [options]\n");
    printf("Commands:\n");
    printf("  start   Start the transfer daemon\n");
    printf("  stop    Stop the transfer daemon\n");
    printf("  status  Show the status of the transfer daemon\n");
    printf("Options:\n");
    printf("  -t + <type>,   Specify the transfer daemon type (UART, SOCKET)\n");
    printf("  -s + <type>,   Specify the source type (YOLO, GPIO)\n");
    printf("  -c,   Show configuration options\n");
    printf("  -h,   Show this help message\n");
    printf("  -v,   Show version information\n");
    printf("  -d,   Show debugging information\n");
}

int debug_daemon()
{
    printf("Monitoring log file in real-time. Press Ctrl+C to exit.\n");

    FILE *log_file = fopen(LOG_FILE, "r");
    if (log_file == NULL)
    {
        perror("fopen (log file)");
        return -1;
    }

    if (fseek(log_file, 0, SEEK_END) != 0)
    {
        perror("fseek (log file)");
        fclose(log_file);
        return -1;
    }

    while (1)
    {
        char line[256];
        // Leer todas las líneas nuevas disponibles
        while (fgets(line, sizeof(line), log_file) != NULL)
        {
            printf("%s", line);
            fflush(stdout);
        }

        // IMPORTANTE: Limpiar eof para que se puedan detectar cambios
        clearerr(log_file);

        usleep(200000);
    }

    fclose(log_file);
    return 0;
}

void version()
{
    printf("Transfer Daemon Tool 1.0\n");
    printf("Copyright (C) 2025 felix@cosmOS\n");
    printf("License: GPLv3\n");
}

int transferd_loop()

{

    system("echo 3 > /proc/sys/kernel/printk 2>/dev/null"); // quita los logs del kernel

    if (current_config.source_type == SOURCE_TYPE_YOLO)
    {

        if (yolo_pid == -1) // SI no esta CORRIENDO EL yolamen
        {
            log_error("Not running, creating PID: %d", yolo_pid);
            int detection_pipe_fds[2]; 

            if (pipe(detection_pipe_fds) == -1)
            { // If pipe fails, return.
                log_error("TRANSFERD_LOOP: Failed to create pipe for YOLO: %s", strerror(errno));
                return;
            }

            yolo_pid = fork();

            if (yolo_pid == -1)
            {
                log_error("TRANSFERD_LOOP: Failed to fork for YOLO: %s", strerror(errno));
                close(detection_pipe_fds[0]);
                close(detection_pipe_fds[1]);
                return;
            }
            else if (yolo_pid == 0) // CHILD
            { 

                log_message("Yolo Child from PID");
                const char *yolo_executable_dir = "/usr/bin"; 

                close(detection_pipe_fds[0]); // i closed the reader for the child. only writer.

                int dev_null_fd = open("/dev/null", O_WRONLY);

                     if (dev_null_fd != -1) {
                    // redirect a stdout
                    if (dup2(dev_null_fd, STDOUT_FILENO) < 0) {
                        log_error("TRANSFERD_LOOP: Child: Failed to dup2 STDOUT_FILENO: %s", strerror(errno));
                    }
                    
                    // stderr al log file
                    int transferd_log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (transferd_log_fd != -1) {
                        if (dup2(transferd_log_fd, STDERR_FILENO) < 0) {
                            log_error("TRANSFERD_LOOP: Child: Failed to dup2 STDERR_FILENO to transferd.log: %s", strerror(errno));
                        }
                        if (transferd_log_fd > STDERR_FILENO) {
                            close(transferd_log_fd);
                        }
                    } else {
                        // dev null otra vez
                        if (dup2(dev_null_fd, STDERR_FILENO) < 0) {
                            log_error("TRANSFERD_LOOP: Child: Failed to dup2 STDERR_FILENO: %s", strerror(errno));
                        }
                    }
                    if (dev_null_fd > STDERR_FILENO) {
                        close(dev_null_fd);
                    }
                } else {
                    log_error("TRANSFERD_LOOP: Child: Failed to open /dev/null for YOLO output redirection: %s", strerror(errno));
                }

                char detection_fd_str[16];
                snprintf(detection_fd_str, sizeof(detection_fd_str), "%d", detection_pipe_fds[1]);
                

                char *const yolo_argv[] = {
                    "YOLO",                      // argv[0]
                    "--detection-fd",            // argv[1]
                    detection_fd_str,            // argv[2]
                    NULL                         // null end
                };

                 if (chdir(yolo_executable_dir) != 0) {
                    log_error("TRANSFERD_LOOP: Child: Failed to chdir to %s: %s", yolo_executable_dir, strerror(errno));
                    close(detection_pipe_fds[1]);
                    _exit(127); 
                }

                execvp("YOLO", yolo_argv); // Aqui se termina to
                log_error("TRANSFERD_LOOP: Failed to exec YOLO process: %s", strerror(errno));
                close(detection_pipe_fds[1]);
                _exit(EXIT_FAILURE);
            }
            else // PARENT
            {                                             
                close(detection_pipe_fds[1]);              // i closed the writer for the parent. only reader.
                yolo_pipe_read_fd = detection_pipe_fds[0]; // save the read end of the pipe
                log_message("TRANSFERD_LOOP: YOLO process started with PID. Father process. %d", yolo_pid);
                int flags_detect = fcntl(yolo_pipe_read_fd, F_GETFL, 0);
                if (flags_detect == -1 || fcntl(yolo_pipe_read_fd, F_SETFL, flags_detect | O_NONBLOCK) == -1)
                {
                    log_error("TRANSFERD_LOOP: fcntl failed for YOLO detection pipe FD %d: %s", yolo_pipe_read_fd, strerror(errno));
                    close(yolo_pipe_read_fd);
                    yolo_pipe_read_fd = -1;
                    kill(yolo_pid, SIGKILL);
                    waitpid(yolo_pid, NULL, 0);
                    yolo_pid = -1;
                }
            }
        }

        if (yolo_pid > 0 && yolo_pipe_read_fd != -1) // PARENT AND PIPE IS OPEN
        {

            log_message("TRANSFERD_LOOP: Reading from YOLO detection pipe as FATHER (FD %d) for PID %d", yolo_pipe_read_fd, yolo_pid);
            char detection_buffer[1024];
            ssize_t bytes_read;

            bytes_read = read(yolo_pipe_read_fd, detection_buffer, sizeof(detection_buffer) - 1);

            if (bytes_read > 0)
            {
                detection_buffer[bytes_read] = '\0';
                char *line = strtok(detection_buffer, "\n");
                while (line != NULL)
                {
                    log_message("YOLO_DETECTION_DATA: %s", line);
                    // TODO
                    line = strtok(NULL, "\n");
                }
            }
               else if (bytes_read == 0)
            { // EOF on detection_pipe
                log_message("TRANSFERD_LOOP: YOLO process (PID %d) closed custom detection pipe (EOF).", yolo_pid);
                close(yolo_pipe_read_fd);
                yolo_pipe_read_fd = -1;

                int status;
                pid_t result = waitpid(yolo_pid, &status, WNOHANG);
                
                if (result == yolo_pid) { 
                    // yolo a la mierda
                    log_message("TRANSFERD_LOOP: YOLO process (PID %d) confirmed exited with status %d.", yolo_pid, status);
                    yolo_pid = -1; // Allow restart
                } 
                else if (result == 0) { 
                    // 
                    log_message("TRANSFERD_LOOP: YOLO (PID %d) is still running but pipe closed unexpectedly. Terminating for clean restart.", yolo_pid);
                    
                    // limpieza
                    if (kill(yolo_pid, SIGTERM) == 0) {
                        usleep(200000); // 200ms
                        result = waitpid(yolo_pid, &status, WNOHANG);
                        if (result == yolo_pid) {
                            log_message("TRANSFERD_LOOP: YOLO (PID %d) terminated gracefully.", yolo_pid);
                        } else if (result == 0) {
                            log_message("TRANSFERD_LOOP: YOLO (PID %d) didn't respond to SIGTERM. Using SIGKILL.", yolo_pid);
                            kill(yolo_pid, SIGKILL);
                            waitpid(yolo_pid, &status, 0);
                        }
                    }
                    yolo_pid = -1; // restart
                } 
                else { // waitpid error
                    log_error("TRANSFERD_LOOP: waitpid error for YOLO PID %d: %s", yolo_pid, strerror(errno));
                    yolo_pid = -1;
                }
            }
            else
            { // bytes_read == -1
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // No data available right now.
                }
                else
                {
                    log_error("TRANSFERD_LOOP: Error reading from YOLO pipe FD %d: %s", yolo_pipe_read_fd, strerror(errno));
                    close(yolo_pipe_read_fd);
                    yolo_pipe_read_fd = -1;
                    if (yolo_pipe_read_fd != -1)
                    {
                        close(yolo_pipe_read_fd);
                        yolo_pipe_read_fd = -1;
                    }
                    kill(yolo_pid, SIGTERM);
                    waitpid(yolo_pid, NULL, 0);
                    yolo_pid = -1;
                }
            }
        }
    }

    return 0;

}

int transferd_logic_init()
{
    yolo_pipe_read_fd = -1;
    yolo_pid = -1;

    log_message("TRANSFERD: Initializing transfer logic...");

    if (load_config_from_file(&current_config) != 0)
    {
        log_error("TRANSFERD: Failed to load configuration. Using default values.");
        return -1;
    }

    if (current_config.output_type == OUTPUT_TYPE_HTTP) {
        if (start_http_server() != 0) {
        log_error("Failed to start HTTP server");
        return -1;
        }
    }
    

    log_message("TRANSFERD: Configuration loaded successfully. Source: %s",
                source_type_to_string(current_config.source_type));
    return 0;
}



int main(int argc, char *argv[])
{
    if (strcmp(argv[1], "start") == 0)
    {
        start_daemon();
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
        stop_daemon();
    }
    else if (strcmp(argv[1], "status") == 0)
    {
        FILE *fp = fopen(PID_FILE, "r");
        if (fp == NULL)
        {
            printf("Daemon is not running (PID file not found).\n");
        }
        else
        {
            pid_t pid;
            if (fscanf(fp, "%d", &pid) != 1)
            {
                printf("Daemon is not running (PID file corrupted).\n");
            }
            else
            {
                printf("Daemon is running with PID: %d\n", pid);
            }
            fclose(fp);
        }
    }
    else if (strcmp(argv[1], "-h") == 0)
    {
        usage();
    }
    else if (strcmp(argv[1], "-v") == 0)
    {
        version();
    }
    else if (strcmp(argv[1], "-d") == 0)
    {
        debug_daemon();
    }
    else if (strcmp(argv[1], "-s") == 0)
    {
        if (strcmp(argv[2], "YOLO") == 0)
        {
            printf("Source type set to YOLO. Restart daemon for changes\n");
            current_config.source_type = SOURCE_TYPE_YOLO;
            if (save_config_to_file(&current_config) != 0)
            {
                printf("Failed to save configuration.\n");
                return 1;
            }
        }
        else if (strcmp(argv[2], "GPIO") == 0)
        {
            printf("Source type set to GPIO. Restart daemon for changes\n");
            current_config.source_type = SOURCE_TYPE_GPIO;
            if (save_config_to_file(&current_config) != 0)
            {
                printf("Failed to save configuration.\n");
                return 1;
            }
        }
        else
        {
            printf("Invalid source type. Use 'YOLO' or 'GPIO'.\n");
        }
    }
    else
    {
        usage();
        return 1;
    }
    return 0;
}