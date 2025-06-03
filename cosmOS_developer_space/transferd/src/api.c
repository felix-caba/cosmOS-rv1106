#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include "../include/transferd.h"
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/uio.h>

static int server_fd = -1;
static pthread_t server_thread;
static int server_running = 0;
static char latest_message[1024] = "{\"objects\":[],\"timestamp\":0}";

static pthread_mutex_t latest_message_mutex = PTHREAD_MUTEX_INITIALIZER; // por desgracia
// con un solo hilo no podia manejar varios fetch (stream + api detections) a la vez asi que toca mutex

// Estructura para manejar clientes
typedef struct
{
    int client_fd;
    char request[2048];
} client_request_t;

// pre def
static void send_response(int client_fd, const char *response);
static void handle_detection_api(int client_fd);
static void handle_mjpeg_stream(int client_fd);
static void handle_main_page(int client_fd);
static void *handle_client_thread(void *arg);
static void *http_thread(void *arg);
static void get_device_ip(char *ip_buffer, size_t buffer_size);

static void send_response(int client_fd, const char *response)
{

    send(client_fd, response, strlen(response), 0);
}

static void handle_detection_api(int client_fd)
{
    char safe_message[1024];

    pthread_mutex_lock(&latest_message_mutex);
    strncpy(safe_message, latest_message, sizeof(safe_message) - 1);
    safe_message[sizeof(safe_message) - 1] = '\0';
    pthread_mutex_unlock(&latest_message_mutex);

    char response[2048];
    int content_length = strlen(safe_message);

    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Cache-Control: no-cache, no-store, must-revalidate\r\n"
             "Pragma: no-cache\r\n"
             "Expires: 0\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             content_length, safe_message);
    send_response(client_fd, response);
}

static void handle_temp_api(int client_fd)
{
    char *temp_reading = get_temp_reading();

    char response[512];
    int content_length = strlen(temp_reading);

    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %d\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Cache-Control: no-cache, no-store, must-revalidate\r\n"
             "Pragma: no-cache\r\n"
             "Expires: 0\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             content_length, temp_reading);
    send_response(client_fd, response);
}

static void handle_temp_page(int client_fd)
{
    char device_ip[64];
    get_device_ip(device_ip, sizeof(device_ip));

    char response[16384];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=UTF-8\r\n"
             "Connection: close\r\n\r\n"
             "<!DOCTYPE html>"
             "<html><head>"
             "<title>CosmOS Transferd - Temperature Sensor</title>"
             "<meta name='viewport' content='width=device-width, initial-scale=1'>"
             "<style>"
             "body{font-family:Arial,sans-serif;margin:20px;background:#000;color:white;text-align:center;}"
             ".container{max-width:600px;margin:0 auto;}"
             ".temp-display{background:#1a1a1a;padding:40px;border-radius:10px;margin:30px 0;border:2px solid #333;}"
             ".temperature{font-size:4em;font-weight:bold;color:#00ff00;margin:20px 0;}"
             ".status{background:#1a5f1a;padding:15px;border-radius:5px;margin:20px 0;}"
             ".info{background:#333;padding:15px;border-radius:5px;margin:20px 0;}"
             "h1{color:#00ff00;}"
             ".sensor-type{color:#ccc;font-size:1.2em;margin-bottom:10px;}"
             "</style>"
             "</head><body>"
             "<div class='container'>"
             "<h1>CosmOS Temperature Monitor</h1>"
             "<div class='temp-display'>"
             "<div class='sensor-type'>LM75 I2C Temperature Sensor</div>"
             "<div id='temperature' class='temperature'>Loading...</div>"
             "</div>"
             "<div id='status' class='status'>Reading sensor...</div>"
             "<div class='info'>"
             "<strong>Sensor:</strong> LM75 (I2C Address: 0x48)<br>"
             "<strong>Bus:</strong> /dev/i2c-3<br>"
             "<strong>Resolution:</strong> 0.5°C<br>"
             "<strong>Device IP:</strong> %s<br>"
             "<strong>API URL:</strong> http://%s:8080/api/temperature"
             "</div>"
             "</div>"
             "<script>"
             "var tempElement = document.getElementById('temperature');"
             "var statusElement = document.getElementById('status');"

             "function updateTemperature() {"
             "fetch('/api/temperature')"
             ".then(response => {"
             "if (!response.ok) {"
             "throw new Error('HTTP ' + response.status);"
             "}"
             "return response.text();"
             "})"
             ".then(data => {"
             "if (data.includes('ERROR') || data.includes('WAIT')) {"
             "tempElement.innerHTML = data;"
             "tempElement.style.color = '#ff6600';"
             "statusElement.innerHTML = 'Sensor initializing...';"
             "} else {"
             "tempElement.innerHTML = data;"
             "tempElement.style.color = '#00ff00';"
             "statusElement.innerHTML = 'Sensor active - Updated: ' + new Date().toLocaleTimeString();"
             "}"
             "})"
             ".catch(err => {"
             "console.error('Temperature API error:', err);"
             "tempElement.innerHTML = 'CONNECTION ERROR';"
             "tempElement.style.color = '#ff0000';"
             "statusElement.innerHTML = 'Connection error - retrying...';"
             "});"
             "}"

             "updateTemperature();"
             "setInterval(updateTemperature, 2000);"
             "</script></body></html>",
             device_ip, device_ip);

    send_response(client_fd, response);
}

/*
    Actualiza el mensaje de la API con el nombre del objeto y la marca de tiempo actual
*/
void update_api(const char *object_name)
{
    time_t now = time(NULL);

    log_message("API: Received detection data: '%s'", API_LOG_FILE, object_name);

    pthread_mutex_lock(&latest_message_mutex);

    if (strcmp(object_name, "CLEAR") == 0)
    {
        snprintf(latest_message, sizeof(latest_message),
                 "{\"objects\":[],\"timestamp\":%ld}", now);
        log_message("API: Set CLEAR message", API_LOG_FILE);
        pthread_mutex_unlock(&latest_message_mutex);
        return;
    }

    char json_objects[512] = "[";
    char temp_string[256];
    strncpy(temp_string, object_name, sizeof(temp_string) - 1);
    temp_string[sizeof(temp_string) - 1] = '\0';

    char *newline = strchr(temp_string, '\n');
    if (newline)
        *newline = '\0';

    int first = 1;
    char *token = strtok(temp_string, ":");

    while (token != NULL)
    {
        while (*token == ' ')
            token++;
        int len = strlen(token);
        while (len > 0 && (token[len - 1] == ' ' || token[len - 1] == '\n' || token[len - 1] == '\r'))
        {
            token[--len] = '\0';
        }

        if (strlen(token) > 0)
        {
            if (!first)
            {
                strcat(json_objects, ",");
            }

            strcat(json_objects, "\"");
            strncat(json_objects, token, sizeof(json_objects) - strlen(json_objects) - 3);
            strcat(json_objects, "\"");
            first = 0;

            log_message("API: Added detection: '%s'", API_LOG_FILE, token);
        }

        token = strtok(NULL, ":");
    }

    strcat(json_objects, "]");

    snprintf(latest_message, sizeof(latest_message),
             "{\"objects\":%s,\"timestamp\":%ld}", json_objects, now);

    log_message("API: Final JSON: %s", API_LOG_FILE, latest_message);
    pthread_mutex_unlock(&latest_message_mutex);
}

static void get_device_ip(char *ip_buffer, size_t buffer_size)
{
    struct ifaddrs *ifaddr, *ifa;

    strncpy(ip_buffer, "unknown", buffer_size - 1);
    ip_buffer[buffer_size - 1] = '\0';

    if (getifaddrs(&ifaddr) == -1)
    {
        return;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in *addr_in = (struct sockaddr_in *)ifa->ifa_addr;
            char *addr_str = inet_ntoa(addr_in->sin_addr);

            if (strncmp(addr_str, "127.", 4) != 0 &&
                (strncmp(ifa->ifa_name, "eth", 3) == 0 ||
                 strncmp(ifa->ifa_name, "wlan", 4) == 0 ||
                 strncmp(ifa->ifa_name, "en", 2) == 0))
            {
                strncpy(ip_buffer, addr_str, buffer_size - 1);
                ip_buffer[buffer_size - 1] = '\0';
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
}

static void handle_mjpeg_stream(int client_fd)
{
    printf("=== MJPEG STREAM REQUEST RECEIVED ===\n");

    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char test_recv;
    int recv_result = recv(client_fd, &test_recv, 1, MSG_DONTWAIT | MSG_PEEK);
    if (recv_result == 0)
    {
        log_message("Client disconnected before stream start\n", API_LOG_FILE);
        return;
    }
    else if (recv_result < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        log_message("Client connection error before stream start: %s\n", API_LOG_FILE, strerror(errno));
        return;
    }

    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: keep-alive\r\n\r\n";

    if (send(client_fd, header, strlen(header), MSG_NOSIGNAL) <= 0)
    {
        log_message("Failed to send MJPEG header\n", API_LOG_FILE);
        return;
    }

    log_message("Waiting for first valid frame...\n", API_LOG_FILE);
    int init_attempts = 0;
    const int MAX_INIT_ATTEMPTS = 50; // 5 segundos max

    while (init_attempts < MAX_INIT_ATTEMPTS)
    {
        FILE *jpg = fopen("/tmp/frame.jpg", "rb");
        if (jpg)
        {
            fseek(jpg, 0, SEEK_END);
            long jpg_size = ftell(jpg);

            if (jpg_size > 10000 && jpg_size < 1048576)
            { 
                fseek(jpg, 0, SEEK_SET);
                
                unsigned char jpeg_header[2];
                if (fread(jpeg_header, 1, 2, jpg) == 2 &&
                    jpeg_header[0] == 0xFF && jpeg_header[1] == 0xD8)
                {
                    fclose(jpg);
                    log_message("First valid frame found after %d attempts\n", API_LOG_FILE, init_attempts);
                    break;
                }
            }
            fclose(jpg);
        }

        init_attempts++;
        usleep(100000); // 100ms entre intentos
    }

    if (init_attempts >= MAX_INIT_ATTEMPTS)
    {
        printf("No valid frame found after %d attempts, starting anyway\n", init_attempts);
    }

    uint32_t frame_count = 0;
    int consecutive_failures = 0;
    const int MAX_FAILURES = 20; 

    while (server_running && consecutive_failures < MAX_FAILURES)
    {
        char test_byte;
        int recv_result = recv(client_fd, &test_byte, 1, MSG_DONTWAIT | MSG_PEEK);
        if (recv_result == 0)
        {
            log_message("Client disconnected during stream (frame %u)\n", API_LOG_FILE, frame_count);
            break;
        }
        else if (recv_result < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            log_message("Client connection error during stream: %s\n", API_LOG_FILE, strerror(errno));
            break;
        }

        FILE *jpg = fopen("/tmp/frame.jpg", "rb");
        if (jpg)
        {
            fseek(jpg, 0, SEEK_END);
            long jpg_size = ftell(jpg);

            if (jpg_size > 5000 && jpg_size < 1048576)
            {
                fseek(jpg, 0, SEEK_SET);

                char *img_buffer = malloc(jpg_size);
                if (img_buffer)
                {
                    size_t bytes_read = fread(img_buffer, 1, jpg_size, jpg);
                    if (bytes_read == jpg_size)
                    {
                        // valid jpeg
                        if (img_buffer[0] == 0xFF && img_buffer[1] == 0xD8 &&
                            img_buffer[jpg_size - 2] == 0xFF && img_buffer[jpg_size - 1] == 0xD9)
                        {
                            char frame_header[256];
                            int header_len = snprintf(frame_header, sizeof(frame_header),
                                                      "\r\n--frame\r\n"
                                                      "Content-Type: image/jpeg\r\n"
                                                      "Content-Length: %ld\r\n\r\n",
                                                      jpg_size);

                            struct iovec iov[2] = {
                                {.iov_base = frame_header, .iov_len = header_len},
                                {.iov_base = img_buffer, .iov_len = jpg_size}};

                            ssize_t sent = writev(client_fd, iov, 2);
                            if (sent == (header_len + jpg_size))
                            {
                                frame_count++;
                                consecutive_failures = 0;

                                if (frame_count % 30 == 0)
                                {
                                    log_message("MJPEG Stream: Sent frame %u (size: %ld bytes)",
                                                API_LOG_FILE, frame_count, jpg_size);
                                }
                            }
                            else if (sent < 0)
                            {
                                if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN)
                                {
                                    printf("Client disconnected: %s\n", strerror(errno));
                                    free(img_buffer);
                                    fclose(jpg);
                                    break;
                                }
                                log_message("Failed to send frame %u: %s", API_LOG_FILE, frame_count, strerror(errno));
                                consecutive_failures++;
                            }
                            else
                            {
                                log_message("Partial send: %zd bytes sent (expected %ld bytes)",
                                            API_LOG_FILE, sent, header_len + jpg_size);
                                consecutive_failures++;
                                free(img_buffer);
                                fclose(jpg);
                                break;
                            }
                        }
                        else
                        {
                            consecutive_failures++;
                            if (consecutive_failures % 10 == 1)
                            {
                                log_message("Invalid JPEG header in frame %u (size: %ld bytes)",
                                            API_LOG_FILE, frame_count, jpg_size);
                            }
                        }
                    }
                    else
                    {
                        consecutive_failures++;
                    }

                    free(img_buffer);
                }
                else // BUFFFFFFFFFEEEEEERRRR
                {
                    consecutive_failures++;
                    log_message("Failed to allocate memory for JPEG frame (size: %ld bytes)",
                                API_LOG_FILE, jpg_size);
                }
            }
            else // inv frame sz
            {
                consecutive_failures++;
                if (consecutive_failures % 20 == 1)
                {
                    log_message("Frame size invalid: %ld bytes (attempt %d)",
                                API_LOG_FILE, jpg_size, consecutive_failures);
                }
            }

            fclose(jpg);
        }
        else
        {
            consecutive_failures++;
            if (consecutive_failures % 50 == 1)
            {
                log_message("Failed to open /tmp/frame.jpg for reading (attempt %d): %s",
                            API_LOG_FILE, consecutive_failures, strerror(errno));
            }
        }

        usleep(33000); // 30fps
    }

    printf("=== MJPEG STREAM ENDED (frames: %u, failures: %d) ===\n",
           frame_count, consecutive_failures);
}

static void handle_main_page(int client_fd)
{
    char device_ip[64];
    get_device_ip(device_ip, sizeof(device_ip));

    char response[32768];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=UTF-8\r\n"
             "Connection: close\r\n\r\n"
             "<!DOCTYPE html>"
             "<html><head>"
             "<title>CosmOS Transferd - YOLO Detection</title>"
             "<meta name='viewport' content='width=device-width, initial-scale=1'>"
             "<style>"
             "body{font-family:Arial,sans-serif;margin:20px;background:#000;color:white;}"
             ".container{max-width:1000px;margin:0 auto;text-align:center;}"
             "#videoElement{width:100%%;max-width:720px;border:2px solid #333;background:#000;display:block;margin:20px auto;}"
             ".status{background:#1a5f1a;padding:15px;border-radius:5px;margin:20px 0;}"
             ".info{background:#333;padding:15px;border-radius:5px;margin:20px 0;}"
             ".detections{background:#2a2a2a;padding:20px;border-radius:5px;margin:20px 0;text-align:left;}"
             ".detection-item{background:#444;padding:10px;margin:5px 0;border-radius:3px;display:flex;justify-content:space-between;}"
             ".detection-name{font-weight:bold;color:#00ff00;}"
             ".detection-time{color:#ccc;font-size:0.9em;}"
             "h1{color:#00ff00;}"
             "h3{color:#00ff00;margin-bottom:10px;}"
             "#detectionsList{min-height:100px;}"
             ".no-detections{color:#999;font-style:italic;text-align:center;padding:20px;}"
             "</style>"
             "</head><body>"
             "<div class='container'>"
             "<h1>CosmOS Transferd - YOLO Detection System</h1>"
             "<img id='videoElement' src='/api/video/stream' alt='Live Video Stream'>"
             "<div id='status' class='status'>MJPEG Stream</div>"
             "<div class='detections'>"
             "<h3>Current Detections:</h3>"
             "<div id='detectionsList'>"
             "<div class='no-detections'>No detections available</div>"
             "</div>"
             "</div>"
             "<div class='info'>"
             "<strong>Mode:</strong> MJPEG with YOLO Object Detection<br>"
             "<strong>Resolution:</strong> 720x480 @ 30fps<br>"
             "<strong>Device IP:</strong> %s<br>"
             "<strong>Stream URL:</strong> http://%s:8080/api/video/stream<br>"
             "<strong>API URL:</strong> http://%s:8080/api/detections"
             "</div>"
             "</div>"
             "<script>"
             "var video = document.getElementById('videoElement');"
             "var status = document.getElementById('status');"
             "var detectionsList = document.getElementById('detectionsList');"

             "var video = document.getElementById('videoElement');"
             "var status = document.getElementById('status');"
             "var detectionsList = document.getElementById('detectionsList');"
             "var reconnectAttempts = 0;"
             "var maxReconnectAttempts = 5;"

             "function setupVideoStream() {"
             "video.src = '/api/video/stream?t=' + Date.now();"
             "status.innerHTML = 'Connecting to stream...';"
             "status.style.background = '#cc6600';"
             "}"

             "video.onload = function() {"
             "status.innerHTML = 'MJPEG Stream Active';"
             "status.style.background = '#1a5f1a';"
             "reconnectAttempts = 0;"
             "};"

             "video.onerror = function(e) {"
             "console.error('Video stream error:', e);"
             "reconnectAttempts++;"

             "if (reconnectAttempts <= maxReconnectAttempts) {"
             "status.innerHTML = 'Stream Error - Reconnecting (' + reconnectAttempts + '/' + maxReconnectAttempts + ')...';"
             "status.style.background = '#cc0000';"

             "setTimeout(function() {"
             "setupVideoStream();"
             "}, 2000 * reconnectAttempts);" // Delay incremental
             "} else {"
             "status.innerHTML = 'Stream Failed - Please refresh page';"
             "status.style.background = '#660000';"
             "}"
             "};"

             "video.onabort = function() {"
             "console.log('Video stream aborted');"
             "if (reconnectAttempts < maxReconnectAttempts) {"
             "setTimeout(setupVideoStream, 1000);"
             "}"
             "};"

             // Detectar cuando la imagen se carga por primera vez
             "video.addEventListener('loadstart', function() {"
             "status.innerHTML = 'Loading stream...';"
             "status.style.background = '#cc6600';"
             "});"

             "setupVideoStream();" // Iniciar el stream
             "</script></body></html>",
             device_ip, device_ip, device_ip);

    send_response(client_fd, response);
}

/**
 * Enrutador rocho nefasto
 */
static void *handle_client_thread(void *arg)
{
    client_request_t *req = (client_request_t *)arg;
    int client_fd = req->client_fd;
    char *buffer = req->request;

    // routing
    if (current_config.source_type == SOURCE_TYPE_I2C_TEMP)
    {
        if (strstr(buffer, "GET /api/temperature"))
        {
            handle_temp_api(client_fd);
        }
        else if (strstr(buffer, "GET /"))
        {
            handle_temp_page(client_fd);
        }
        else
        {
            const char *not_found = // not found
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n\r\n"
                "404 Not Found";
            send_response(client_fd, not_found);
        }
    }
    else
    { // MODO YOLO!!!!!!!!!!!!!!!!!!
        if (strstr(buffer, "GET /api/detections"))
        {
            handle_detection_api(client_fd);
        }
        else if (strstr(buffer, "GET /api/video/stream"))
        {
            handle_mjpeg_stream(client_fd);
        }
        else if (strstr(buffer, "GET /"))
        {
            handle_main_page(client_fd);
        }
        else
        {
            const char *not_found =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n\r\n"
                "404 Not Found";
            send_response(client_fd, not_found);
        }
    }

    close(client_fd);
    free(req);
    return NULL;
}

static void *http_thread(void *arg)
{
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        log_message("Failed to create socket: %s", API_LOG_FILE, strerror(errno));
        return NULL;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        log_message("Failed to set socket options: %s", API_LOG_FILE, strerror(errno));
        close(server_fd);
        return NULL;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        log_message("Failed to bind socket: %s", API_LOG_FILE, strerror(errno));
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 10) < 0)
    {
        log_message("Failed to listen on socket: %s", API_LOG_FILE, strerror(errno));
        close(server_fd);
        return NULL;
    }

    log_message("HTTP Server started on port 8080", API_LOG_FILE);

    while (server_running)
    {
        int client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_fd < 0)
        {
            if (server_running)
            {
                printf("Accept failed: %s\n", strerror(errno));
            }
            continue;
        }

        // estructura con malloc para el cliente CUIDADO CON LOS MEMORY LEAK QUE ME LA HAN LIADO 500 veces
        client_request_t *req = malloc(sizeof(client_request_t));
        if (!req)
        {
            close(client_fd);
            continue;
        }

        req->client_fd = client_fd;
        ssize_t bytes_read = read(client_fd, req->request, sizeof(req->request) - 1);
        if (bytes_read <= 0)
        {
            close(client_fd);
            free(req);
            continue;
        }
        req->request[bytes_read] = '\0';

        // hilo de peticion
        pthread_t client_thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&client_thread, &attr, handle_client_thread, req) != 0)
        {
            //
            handle_client_thread(req);
        }

        pthread_attr_destroy(&attr);
    }

    return NULL;
}

int start_http_server(void)
{
    if (server_running)
    {
        log_message("HTTP server already running", API_LOG_FILE);
        return 0;
    }

    server_running = 1;

    if (pthread_create(&server_thread, NULL, http_thread, NULL) != 0)
    {
        log_message("Failed to create HTTP server thread: %s", API_LOG_FILE, strerror(errno));
        server_running = 0;
        return -1;
    }

    pthread_detach(server_thread);
    log_message("HTTP server thread created successfully", API_LOG_FILE);
    return 0;
}

void stop_http_server(void)
{
    if (!server_running)
    {
        return;
    }

    server_running = 0;

    if (server_fd >= 0)
    {
        shutdown(server_fd, SHUT_RDWR);
        close(server_fd);
        server_fd = -1;
    }

    unlink("/tmp/jpeg_stream");
    log_message("HTTP server stopped", API_LOG_FILE);
}