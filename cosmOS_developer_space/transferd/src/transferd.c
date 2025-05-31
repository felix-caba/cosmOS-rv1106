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
static char last_detection[32] = ""; // Caché para no actualizar la pantalla si no hay cambios
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
    printf("  -l + <type> + <option>,   Manage logs (trans, yolo, api) with options (delete, view)\n");
    printf("  -o + <type>,   Specify the transfer output (HTTPS, SCREEN)\n");
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
void update_output(const char* object_name) {

    if (object_name == NULL || strlen(object_name) == 0) {
        log_message("Received empty object name, skipping update.", LOG_FILE);
        return;
    }

    if (last_detection[0] != '\0' && strcmp(last_detection, object_name) == 0) {
        log_message("No change in detection, skipping update for: %s", LOG_FILE, object_name);
        return; // No hay cambio, no actualizamos
    }

    if (current_config.output_type == OUTPUT_TYPE_HTTP) {
        update_api(object_name); 
    }
    if (current_config.output_type == OUTPUT_TYPE_SCREEN) {
        update_screen(object_name);     
    }

    log_message("Updated output with detection: %s", LOG_FILE, object_name);
    strncpy(last_detection, object_name, sizeof(last_detection) - 1);
    last_detection[sizeof(last_detection) - 1] = '\0'; 

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
                return;
            }

            yolo_pid = fork();

            if (yolo_pid == -1)
            {
                log_message("TRANSFERD_LOOP: Failed to fork YOLO process: %s", LOG_FILE, strerror(errno));
                close(detection_pipe_fds[0]);
                close(detection_pipe_fds[1]);
                return;
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
                    "YOLO",           // argv[0]
                    "--detection-fd", // argv[1]
                    detection_fd_str, // argv[2]
                    NULL              // null end
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

        if (yolo_pid > 0 && yolo_pipe_read_fd != -1) // Accede el padre y la pipe está abierta
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
        // Todo
    }

    return 0;
}

/*
    Inicializa la lógica de transferencia, carga la configuración y arranca los servicios necesarios.
*/
int transferd_logic_init()
{
    yolo_pipe_read_fd = -1;
    yolo_pid = -1;

    log_message("TRANSFERD: Initializing transfer logic...", LOG_FILE);

    if (load_config_from_file(&current_config) != 0)
    {
        log_message("TRANSFERD: Failed to load configuration file. Using default values.", LOG_FILE);
        return -1;
    }

    if (current_config.output_type == OUTPUT_TYPE_HTTP)  // Init del servidor HTTP
    {
        if (start_http_server() != 0)
        {
            log_message("TRANSFERD: Failed to start HTTP server. Exiting.", LOG_FILE);
            return -1;
        }
    }

    if (current_config.output_type == OUTPUT_TYPE_SCREEN)   // Init de la pantalla 
    {
        if (start_screen() != 0)
        {
            log_message("TRANSFERD: Failed to start screen output. Exiting.", LOG_FILE);
            return -1;
        }
    }

    log_message("TRANSFERD: Configuration loaded successfully. Source: %s",
                source_type_to_string(current_config.source_type), LOG_FILE);
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
            printf("Please specify the log option (delete, view) and the file type (TRANSFERD, YOLO, API).\n");
            return 1;
        }

        if (strcmp(argv[2], "trans") == 0)
        {
            if (strcmp(argv[3], "delete") == 0)
            {
                printf("Deleting transferd log file.\n");
                remove(LOG_FILE);
            }
            else if (strcmp(argv[3], "view") == 0)
            {
                printf("Viewing transferd log file:\n");
                system("cat " LOG_FILE);
            }
            else
            {
                printf("Invalid option for TRANSFERD log. Use 'delete' or 'view'.\n");
            }
        }
        else if (strcmp(argv[2], "yolo") == 0)
        {
            if (strcmp(argv[3], "delete") == 0)
            {
                printf("Deleting YOLO log file.\n");
                remove(YOLO_LOG_FILE);
            }
            else if (strcmp(argv[3], "view") == 0)
            {
                printf("Viewing YOLO log file:\n");
                system("cat " YOLO_LOG_FILE);
            }
            else
            {
                printf("Invalid option for YOLO log. Use 'delete' or 'view'.\n");
            }
        }
        else if (strcmp(argv[2], "api") == 0)
        {
            if (strcmp(argv[3], "delete") == 0)
            {
                printf("Deleting API log file.\n");
                remove(API_LOG_FILE);
            }
            else if (strcmp(argv[3], "view") == 0)
            {
                printf("Viewing API log file:\n");
                system("cat " API_LOG_FILE);
            }
            else
            {
                printf("Invalid option for API log. Use 'delete' or 'view'.\n");
            }
        }
        else
        {
            printf("Invalid log type. Use 'TRANSFERD', 'YOLO', or 'API'.\n");
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

        // Cargar configuración actual primero
        if (load_config_from_file(&current_config) != 0)
        {
            // Si no existe config, usar valores por defecto
            current_config.source_type = SOURCE_TYPE_YOLO;
            current_config.output_type = OUTPUT_TYPE_HTTP;
        }

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
    else if (strcmp(argv[1], "-c") == 0)
    {
        printf("Current configuration:\n");
        system("cat /etc/transferd.conf");
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
            printf("Please specify the source type (YOLO or GPIO).\n");
            return 1;
        }

        if (load_config_from_file(&current_config) != 0)
        {
            current_config.source_type = SOURCE_TYPE_YOLO;
            current_config.output_type = OUTPUT_TYPE_HTTP;
        }

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
            current_config.source_type = SOURCE_TYPE_I2C_TEMP;
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