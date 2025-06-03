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
#include <linux/i2c-dev.h>

static detection_state_t current_state;
static detection_state_t previous_state;

transferd_config_t current_config;
static char last_detections[32] = ""; // Caché para no actualizar la pantalla si no hay cambios
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
    printf("  -l + <type>,   View logs (transferd, yolo, api, screen)\n");
    printf("  -o + <type>,   Specify the transfer output (http, screen)\n");
    printf("  -s + <type>,   Specify the source type (yolo, i2c)\n");

    printf("  -cc,  Show current configuration, if daemon is running\n");
    printf("  -cf,  Show configuration file\n");
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
        while (fgets(line, sizeof(line), log_file) != NULL)
        {
            printf("%s", line);
            fflush(stdout);
        }

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

/*
    Actualiza el output según la configuración actual, HTTP o Screen, solo si hay un cambio en la detección.
    Así no se jode la pantalla con refrescos constantes.
*/
void update_output(const char *objects_name)
{

    if (objects_name == NULL || strlen(objects_name) == 0)
    {
        log_message("Received empty objects name, skipping update.", LOG_FILE);
        return;
    }

    if (last_detections[0] != '\0' && strcmp(last_detections, objects_name) == 0)
    {
        log_message("No change in detection, skipping update for: %s", LOG_FILE, objects_name);
        return; // No hay cambio, no actualizamos
    }

    if (current_config.output_type == OUTPUT_TYPE_HTTP)
    {
        update_api(objects_name);
    }
    if (current_config.output_type == OUTPUT_TYPE_SCREEN)
    {
        update_screen(objects_name);
    }

    log_message("TRANSFERD: Last Det: %s", LOG_FILE, last_detections);
    log_message("Updated output with detection: %s", LOG_FILE, objects_name);
    strncpy(last_detections, objects_name, sizeof(last_detections) - 1);
    last_detections[sizeof(last_detections) - 1] = '\0';
}

int transferd_loop()
{

    if (current_config.source_type == SOURCE_TYPE_YOLO)
    {

        if (yolo_pid == -1) // Si yolo no está corriendo
        {
            log_message("Not running, creating PID: %d", LOG_FILE, yolo_pid);
            int detection_pipe_fds[2];

            if (pipe(detection_pipe_fds) == -1)
            { // If pipe fails, return.
                log_message("TRANSFERD_LOOP: Failed to create detection pipe: %s", LOG_FILE, strerror(errno));
                return -1;
            }

            yolo_pid = fork();

            if (yolo_pid == -1)
            {
                log_message("TRANSFERD_LOOP: Failed to fork YOLO process: %s", LOG_FILE, strerror(errno));
                close(detection_pipe_fds[0]);
                close(detection_pipe_fds[1]);
                return -1;
            }
            else if (yolo_pid == 0) // CHILD
            {

                log_message("Yolo Child from PID", LOG_FILE);
                const char *yolo_executable_dir = "/usr/bin";

                close(detection_pipe_fds[0]); // i closed the reader for the child. only writer.

                int dev_null_fd = open("/dev/null", O_WRONLY);

                if (dev_null_fd != -1)
                {
                    if (dup2(dev_null_fd, STDOUT_FILENO) < 0)
                    {
                        log_message("TRANSFERD_LOOP: Child: Failed to redirect stdout to /dev/null: %s", LOG_FILE, strerror(errno));
                    }

                    if (dev_null_fd > STDERR_FILENO)
                    {
                        close(dev_null_fd);
                    }
                }
                else
                {
                    log_message("TRANSFERD_LOOP: Child: Failed to open /dev/null for YOLO output redirection: %s", LOG_FILE, strerror(errno));
                }

                char detection_fd_str[16];
                snprintf(detection_fd_str, sizeof(detection_fd_str), "%d", detection_pipe_fds[1]);

                char *const yolo_argv[] = {
                    "YOLO",                                                          // argv[0]
                    "--detection-fd",                                                // argv[1]
                    detection_fd_str,                                                // argv[2]
                    current_config.output_type == OUTPUT_TYPE_HTTP ? "--web" : NULL, // argv[3]
                    NULL                                                             // null end
                };

                if (chdir(yolo_executable_dir) != 0)
                {
                    log_message("TRANSFERD_LOOP: Child: Failed to change directory to YOLO executable directory: %s", LOG_FILE, strerror(errno));
                    close(detection_pipe_fds[1]);
                    _exit(127);
                }

                execvp("YOLO", yolo_argv); // Aqui se termina to
                log_message("TRANSFERD_LOOP: Failed to exec YOLO process: %s", LOG_FILE, strerror(errno));
                close(detection_pipe_fds[1]);
                _exit(EXIT_FAILURE);
            }
            else // PARENT
            {
                close(detection_pipe_fds[1]);              // i closed the writer for the parent. only reader.
                yolo_pipe_read_fd = detection_pipe_fds[0]; // save the read end of the pipe
                log_message("TRANSFERD_LOOP: YOLO process started with PID. Father process. %d", LOG_FILE, yolo_pid);
                int flags_detect = fcntl(yolo_pipe_read_fd, F_GETFL, 0);
                if (flags_detect == -1 || fcntl(yolo_pipe_read_fd, F_SETFL, flags_detect | O_NONBLOCK) == -1)
                {
                    log_message("TRANSFERD_LOOP: Failed to set YOLO detection pipe (FD %d) to non-blocking mode: %s", LOG_FILE, yolo_pipe_read_fd, strerror(errno));
                    close(yolo_pipe_read_fd);
                    yolo_pipe_read_fd = -1;
                    kill(yolo_pid, SIGKILL);
                    waitpid(yolo_pid, NULL, 0);
                    yolo_pid = -1;
                }
            }
        }

        if (yolo_pid > 0 && yolo_pipe_read_fd != -1) // Accede el padre y la pipe está abierta, siguiente iteracion bucle
        {

            char detection_buffer[1024];
            ssize_t bytes_read;

            bytes_read = read(yolo_pipe_read_fd, detection_buffer, sizeof(detection_buffer) - 1);

            if (bytes_read > 0)
            {
                detection_buffer[bytes_read] = '\0';
                char *line = strtok(detection_buffer, "\n");
                while (line != NULL)
                {

                    log_message("YOLO_DETECTION_DATA: %s", LOG_FILE, line);

                    update_output(line);

                    line = strtok(NULL, "\n");
                }
            }
            else if (bytes_read == 0)
            { // EOF on detection_pipe
                log_message("TRANSFERD_LOOP: YOLO process (PID %d) closed custom detection pipe (EOF).", LOG_FILE, yolo_pid);
                close(yolo_pipe_read_fd);
                yolo_pipe_read_fd = -1;

                int status;
                pid_t result = waitpid(yolo_pid, &status, WNOHANG);

                if (result == yolo_pid)
                {
                    // yolo a la mierda
                    log_message("TRANSFERD_LOOP: YOLO process (PID %d) confirmed exited with status %d.", LOG_FILE, yolo_pid, status);
                    yolo_pid = -1; // Allow restart
                }
                else if (result == 0)
                {
                    //
                    log_message("TRANSFERD_LOOP: YOLO (PID %d) is still running but pipe closed unexpectedly. Terminating for clean restart.", LOG_FILE, yolo_pid);

                    // limpieza
                    if (kill(yolo_pid, SIGTERM) == 0)
                    {
                        usleep(200000); // 200ms
                        result = waitpid(yolo_pid, &status, WNOHANG);
                        if (result == yolo_pid)
                        {
                            log_message("TRANSFERD_LOOP: YOLO (PID %d) terminated gracefully.", LOG_FILE, yolo_pid);
                        }
                        else if (result == 0)
                        {
                            log_message("TRANSFERD_LOOP: YOLO (PID %d) didn't respond to SIGTERM. Using SIGKILL.", LOG_FILE, yolo_pid);
                            kill(yolo_pid, SIGKILL);
                            waitpid(yolo_pid, &status, 0);
                        }
                    }
                    yolo_pid = -1; // restart
                }
                else
                { // waitpid error
                    log_message("TRANSFERD_LOOP: waitpid failed for YOLO process (PID %d): %s", LOG_FILE, yolo_pid, strerror(errno));
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
                    log_message("TRANSFERD_LOOP: Error reading from YOLO detection pipe (FD %d): %s", LOG_FILE, yolo_pipe_read_fd, strerror(errno));
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

    if (current_config.source_type == SOURCE_TYPE_I2C_TEMP)
    {

        char *temp_str = get_temp_reading();
        if (temp_str != NULL && strlen(temp_str) > 0)
        {
            log_message("TRANSFERD_LOOP: Temperature reading: %s", LOG_FILE, temp_str);
            update_output(temp_str);
        }
        else
        {
            log_message("TRANSFERD_LOOP: Failed to read temperature or no data available.", LOG_FILE);
        }
    }

    return 0;
}

/*
    logica de trans
*/
int transferd_logic_init()
{
    yolo_pipe_read_fd = -1;
    yolo_pid = -1;

    log_message("TRANSFERD: Initializing transfer logic...", LOG_FILE);

    load_config_from_file(&current_config);

    log_message("TRANSFERD: Loaded configuration: Source: %s, Output: %s", LOG_FILE,
                source_type_to_string(current_config.source_type),
                output_type_to_string(current_config.output_type));

    if (current_config.source_type == SOURCE_TYPE_I2C_TEMP)
    { // Init del mega sensor lm75 de temps
        if (start_temp_sensor() != 0)
        {
            log_message("TRANSFERD: Failed to start temperature sensor. Exiting.", LOG_FILE);
            return -1;
        }
    }

    if (current_config.output_type == OUTPUT_TYPE_HTTP) // Init del servidor HTTP
    {
        if (start_http_server() != 0)
        {
            log_message("TRANSFERD: Failed to start HTTP server. Exiting.", LOG_FILE);
            return -1;
        }
    }

    if (current_config.output_type == OUTPUT_TYPE_SCREEN) // Init de la pantalla
    {
        log_message("TRANSFERD: Starting screen output...", LOG_FILE);
        if (start_screen() != 0)
        {
            log_message("TRANSFERD: Failed to start screen output. Exiting.", LOG_FILE);
            return -1;
        }
    }

    log_message("TRANSFERD: Configuration loaded successfully. Source: %s", LOG_FILE,
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
    else if (strcmp(argv[1], "-l") == 0)
    {
        if (argc < 3)
        {
            printf("Please specify the log option (delete, view) and the file type (transferd, yolo, api, screen).\n");
            return 1;
        }
        if (strcmp(argv[2], "transferd") == 0)
        {
            system("cat " LOG_FILE);
        }
        else if (strcmp(argv[2], "yolo") == 0)
        {
            system("cat " YOLO_LOG_FILE);
        }
        else if (strcmp(argv[2], "api") == 0)
        {
            system("cat " API_LOG_FILE);
        }
        else if (strcmp(argv[2], "screen") == 0)
        {
            system("cat " SCREEN_LOG_FILE);
        }
        return 0;
    }
    else if (strcmp(argv[1], "-o") == 0)
    {
        if (argc < 3)
        {
            printf("Please specify the output type (HTTP or SCREEN).\n");
            return 1;
        }

        load_config_from_file(&current_config);

        if (strcmp(argv[2], "HTTP") == 0)
        {
            printf("Output type set to HTTP. Restart daemon for changes\n");
            current_config.output_type = OUTPUT_TYPE_HTTP;
            if (save_config_to_file(&current_config) != 0)
            {
                printf("Failed to save configuration.\n");
                return 1;
            }
        }
        else if (strcmp(argv[2], "SCREEN") == 0)
        {
            printf("Output type set to SCREEN. Restart daemon for changes\n");
            current_config.output_type = OUTPUT_TYPE_SCREEN;
            if (save_config_to_file(&current_config) != 0)
            {
                printf("Failed to save configuration.\n");
                return 1;
            }
        }
        else
        {
            printf("Invalid output type. Use 'HTTP' or 'SCREEN'.\n");
        }
    }
    else if (strcmp(argv[1], "-cf") == 0)
    {
        printf("Current configuration file:\n");
        system("cat /etc/transferd.conf");
    }
    else if (strcmp(argv[1], "-cc") == 0)
    {
        load_config_from_file(&current_config);
        printf("Current configuration:\n");
        printf("Source Type: %s\n", source_type_to_string(current_config.source_type));
        printf("Output Type: %s\n", output_type_to_string(current_config.output_type));
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
        if (argc < 3)
        {
            printf("Please specify the source type (yolo or i2c).\n");
            return 1;
        }

        load_config_from_file(&current_config);

        if (strcmp(argv[2], "yolo") == 0)
        {
            printf("Source type set to yolo. Restart daemon for changes\n");
            current_config.source_type = SOURCE_TYPE_YOLO;
            if (save_config_to_file(&current_config) != 0)
            {
                printf("Failed to save configuration.\n");
                return 1;
            }
        }
        else if (strcmp(argv[2], "i2c") == 0)
        {
            printf("Source type set to i2c. Restart daemon for changes\n");
            current_config.source_type = SOURCE_TYPE_I2C_TEMP;
            if (save_config_to_file(&current_config) != 0)
            {
                printf("Failed to save configuration.\n");
                return 1;
            }
        }
        else
        {
            printf("Invalid source type. Use 'yolo' or 'i2c'.\n");
        }
    }
    else
    {
        usage();
        return 1;
    }
    return 0;
}