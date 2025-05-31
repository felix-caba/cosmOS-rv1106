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

static int server_fd = -1;
static pthread_t server_thread;
static int server_running = 0;
static char latest_message[256] = "";

void update_api(const char *object_name)
{
    time_t now = time(NULL);
    snprintf(latest_message, sizeof(latest_message),
             "{\"object\":\"%s\",\"timestamp\":%ld}", object_name, now);
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

static void send_response(int client_fd, const char *response)
{

    send(client_fd, response, strlen(response), 0);
}

static void handle_detection_api(int client_fd)
{
    char response[1024];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             latest_message);
    send_response(client_fd, response);
}

static void handle_mjpeg_stream(int client_fd)
{
    printf("=== MJPEG STREAM REQUEST RECEIVED ===\n");

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
        printf("Failed to send MJPEG header\n");
        return;
    }
    
    uint32_t frame_count = 0;
    int consecutive_failures = 0;
    const int MAX_FAILURES = 50; // More tolerance for missing files

    // Stream at 15 FPS for ultra-low latency (66ms intervals)
    while (server_running && consecutive_failures < MAX_FAILURES)
    {
        FILE *jpg = fopen("/tmp/frame.jpg", "rb");
        if (jpg)
        {
            // Get file size
            fseek(jpg, 0, SEEK_END);
            long jpg_size = ftell(jpg);

            if (jpg_size > 0 && jpg_size < 1048576) // Max 1MB safety
            {
                fseek(jpg, 0, SEEK_SET);

                // Allocate buffer for the image
                char *img_buffer = malloc(jpg_size);
                if (img_buffer)
                {
                    size_t bytes_read = fread(img_buffer, 1, jpg_size, jpg);
                    if (bytes_read == jpg_size)
                    {
                        // Send boundary header
                        char frame_header[256];
                        int header_len = snprintf(frame_header, sizeof(frame_header),
                                                  "\r\n--frame\r\n"
                                                  "Content-Type: image/jpeg\r\n"
                                                  "Content-Length: %ld\r\n\r\n",
                                                  jpg_size);

                        // Send header + image atomically using writev (fastest method)
                        struct iovec iov[2] = {
                            {.iov_base = frame_header, .iov_len = header_len},
                            {.iov_base = img_buffer, .iov_len = jpg_size}};

                        ssize_t sent = writev(client_fd, iov, 2);
                        if (sent == (header_len + jpg_size))
                        {
                            frame_count++;
                            consecutive_failures = 0;

                            // Log progress every 30 frames
                            if (frame_count % 30 == 0)
                            {
                                printf("Streamed frame %u (%ld bytes) - 15fps mode\n", frame_count, jpg_size);
                            }
                        }
                        else
                        {
                            printf("Client disconnected (sent %zd/%ld bytes)\n", sent, header_len + jpg_size);
                            free(img_buffer);
                            fclose(jpg);
                            break;
                        }
                    }
                    else
                    {
                        consecutive_failures++;
                    }

                    free(img_buffer);
                }
                else
                {
                    consecutive_failures++;
                    printf("Failed to allocate buffer for frame (%ld bytes)\n", jpg_size);
                }
            }
            else
            {
                consecutive_failures++;
                if (jpg_size <= 0)
                {
                    // Empty file - don't spam logs
                }
                else
                {
                    printf("Frame too large: %ld bytes\n", jpg_size);
                }
            }

            fclose(jpg);
        }
        else
        {
            consecutive_failures++;
            // Only log every 100 failures to avoid spam
            if (consecutive_failures % 100 == 1)
            {
                printf("Frame file not available (attempt %d)\n", consecutive_failures);
            }
        }

        // 15 FPS = 66ms delay for ultra-low latency
        usleep(66000);
    }

    printf("=== MJPEG STREAM ENDED (frames: %u) ===\n", frame_count);
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
             "<title>CosmOS Transferd - MJPEG Stream</title>"
             "<meta name='viewport' content='width=device-width, initial-scale=1'>"
             "<style>"
             "body{font-family:Arial,sans-serif;margin:20px;background:#000;color:white;}"
             ".container{max-width:1000px;margin:0 auto;text-align:center;}"
             "#videoElement{width:100%%;max-width:720px;border:2px solid #333;background:#000;display:block;margin:20px auto;}"
             ".status{background:#1a5f1a;padding:15px;border-radius:5px;margin:20px 0;}"
             ".info{background:#333;padding:15px;border-radius:5px;margin:20px 0;}"
             "h1{color:#00ff00;}"
             "</style>"
             "</head><body>"
             "<div class='container'>"
             "<h1>CosmOS Transferd - Live MJPEG Stream</h1>"
             "<img id='videoElement' src='/api/video/stream' alt='Live Video Stream'>"
             "<div id='status' class='status'>MJPEG Stream Active - Ultra Low Latency</div>"
             "<div class='info'>"
             "<strong>Mode:</strong> MJPEG with OpenCV encoding<br>"
             "<strong>Resolution:</strong> 720x480 @ 30fps<br>"
             "<strong>Device IP:</strong> %s<br>"
             "<strong>Stream URL:</strong> http://%s:8080/api/video/stream"
             "</div>"
             "</div>"
             "<script>"
             "var video = document.getElementById('videoElement');"
             "var status = document.getElementById('status');"
             "video.onerror = function() {"
             "status.innerHTML = 'Stream Error - Retrying...';"
             "status.style.background = '#cc0000';"
             "setTimeout(function() {"
             "video.src = '/api/video/stream?' + Date.now();"
             "}, 2000);"
             "};"
             "video.onload = function() {"
             "status.innerHTML = 'MJPEG Stream Active - Ultra Low Latency';"
             "status.style.background = '#1a5f1a';"
             "};"
             "</script></body></html>",
             device_ip, device_ip);

    send_response(client_fd, response);
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

    if (listen(server_fd, 5) < 0)
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

        char buffer[2048] = {0};
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0)
        {
            close(client_fd);
            continue;
        }

        // Route requests
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
            // Send 404 for unknown requests
            const char *not_found =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n\r\n"
                "404 Not Found";
            send_response(client_fd, not_found);
        }

        close(client_fd);
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