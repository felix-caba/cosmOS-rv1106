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

void update_detection(const char *object_name)
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

    if (send(client_fd, header, strlen(header), 0) <= 0)
    {
        printf("Failed to send MJPEG header\n");
        return;
    }

    struct stat pipe_stat;
    if (stat("/tmp/jpeg_stream", &pipe_stat) != 0)
    {
        
        return;
    }

    // Open pipe for reading
    printf("Opening JPEG pipe for reading...\n");
    int jpeg_fd = open("/tmp/jpeg_stream", O_RDONLY | O_NONBLOCK);
    if (jpeg_fd < 0)
    {
        printf("Failed to open JPEG stream pipe: %s\n", strerror(errno));
        return;
    }

    printf("JPEG pipe opened successfully (fd: %d)\n", jpeg_fd);

    uint32_t jpeg_size;
    int frame_count = 0;
    char *jpeg_data = NULL;
    size_t max_frame_size = 1048576; // 1MB max frame size

    while (server_running)
    {
        // Read JPEG size first
        ssize_t bytes_read = read(jpeg_fd, &jpeg_size, sizeof(jpeg_size));

        if (bytes_read == sizeof(jpeg_size))
        {
            // Validate frame size
            if (jpeg_size > 0 && jpeg_size <= max_frame_size)
            {
                // Allocate or reallocate buffer if needed
                if (jpeg_data == NULL || jpeg_size > max_frame_size)
                {
                    free(jpeg_data);
                    jpeg_data = malloc(jpeg_size);
                    if (!jpeg_data)
                    {
                        printf("Failed to allocate memory for JPEG data (%u bytes)\n", jpeg_size);
                        break;
                    }
                }

                // Read JPEG data
                bytes_read = read(jpeg_fd, jpeg_data, jpeg_size);
                if (bytes_read == (ssize_t)jpeg_size)
                {
                    // Send frame boundary
                    char boundary[256];
                    int boundary_len = snprintf(boundary, sizeof(boundary),
                                                "\r\n--frame\r\n"
                                                "Content-Type: image/jpeg\r\n"
                                                "Content-Length: %u\r\n\r\n",
                                                jpeg_size);

                    // Send boundary and JPEG data
                    if (send(client_fd, boundary, boundary_len, MSG_NOSIGNAL) <= 0 ||
                        send(client_fd, jpeg_data, jpeg_size, MSG_NOSIGNAL) <= 0)
                    {
                        printf("Client disconnected during frame transmission\n");
                        break;
                    }

                    frame_count++;
                    if (frame_count % 30 == 0)
                    {
                        printf("Sent MJPEG frame %d (%u bytes)\n", frame_count, jpeg_size);
                    }
                }
                else if (bytes_read == 0)
                {
                    // No more data available, wait a bit
                    usleep(10000); // 10ms
                }
                else
                {
                    printf("Failed to read JPEG data: expected %u, got %zd bytes\n", jpeg_size, bytes_read);
                    usleep(1000);
                }
            }
            else
            {
                printf("Invalid JPEG size: %u bytes (max: %zu)\n", jpeg_size, max_frame_size);
                usleep(1000);
            }
        }
        else if (bytes_read == 0)
        {
            // Pipe closed by writer, wait for reconnection
            usleep(10000); // 10ms
        }
        else if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // No data available yet
                usleep(1000); // 1ms
            }
            else
            {
                printf("Read error: %s\n", strerror(errno));
                break;
            }
        }
        else
        {
            printf("Partial size read: %zd bytes\n", bytes_read);
            usleep(1000);
        }
    }

    // Cleanup
    free(jpeg_data);
    close(jpeg_fd);
    printf("=== MJPEG STREAM REQUEST ENDED ===\n");
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
        log_error("Failed to create socket");
        return NULL;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        log_error("Failed to set socket options");
        close(server_fd);
        return NULL;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        log_error("Failed to bind socket");
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 5) < 0)
    {
        log_error("Failed to listen on socket");
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
        log_error("Failed to create HTTP server thread");
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