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

static int server_fd = -1;
static pthread_t server_thread;
static pthread_t webrtc_thread;
static int server_running = 0;
static int webrtc_running = 0;
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

static const uint8_t mp4_header[] = {
    0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70, // ftyp box
    0x6D, 0x70, 0x34, 0x32, 0x00, 0x00, 0x00, 0x00,
    0x6D, 0x70, 0x34, 0x31, 0x6D, 0x70, 0x34, 0x32,
    0x00, 0x00, 0x00, 0x08, 0x66, 0x72, 0x65, 0x65 // free box
};

static void *http_thread(void *arg)
{
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    log_message("HTTP Server started on port 8080");

    while (server_running)
    {
        int client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_fd < 0)
            continue;

        char buffer[1024] = {0};
        read(client_fd, buffer, 1024);

        char response[32768];

        if (strstr(buffer, "GET /api/detections"))
        {
            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/json\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Connection: close\r\n\r\n"
                     "%s",
                     latest_message);
            send(client_fd, response, strlen(response), 0);
        }

        else if (strstr(buffer, "GET /api/video/stream"))
        {
            printf("=== MJPEG STREAM REQUEST RECEIVED ===\n");

            // MJPEG stream - Ultra fast and compatible
            const char *header =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
                "Cache-Control: no-cache, no-store, must-revalidate\r\n"
                "Pragma: no-cache\r\n"
                "Expires: 0\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: keep-alive\r\n\r\n";

            send(client_fd, header, strlen(header), 0);

            // Verificar que el pipe existe
            struct stat pipe_stat;
            if (stat("/tmp/jpeg_stream", &pipe_stat) != 0)
            {
                printf("ERROR: JPEG pipe does not exist!\n");
                close(client_fd);
                continue;
            }

            // Abrir pipe para lectura
            printf("Opening JPEG pipe for reading...\n");
            int jpeg_fd = open("/tmp/jpeg_stream", O_RDONLY);
            if (jpeg_fd >= 0)
            {
                printf("JPEG pipe opened successfully (fd: %d)\n", jpeg_fd);

                uint32_t jpeg_size;
                int frame_count = 0;

                while (server_running)
                {
                    // Leer tamaño del JPEG
                    ssize_t bytes_read = read(jpeg_fd, &jpeg_size, sizeof(jpeg_size));

                    if (bytes_read == sizeof(jpeg_size))
                    {
                        // Validar tamaño razonable
                        if (jpeg_size > 0 && jpeg_size < 1048576)
                        { // Max 1MB por frame
                            // Leer datos JPEG
                            char *jpeg_data = malloc(jpeg_size);
                            if (jpeg_data)
                            {
                                bytes_read = read(jpeg_fd, jpeg_data, jpeg_size);

                                if (bytes_read == jpeg_size)
                                {
                                    // Enviar frame boundary
                                    char boundary[256];
                                    int boundary_len = snprintf(boundary, sizeof(boundary),
                                                                "\r\n--frame\r\n"
                                                                "Content-Type: image/jpeg\r\n"
                                                                "Content-Length: %u\r\n\r\n",
                                                                jpeg_size);

                                    if (send(client_fd, boundary, boundary_len, MSG_NOSIGNAL) <= 0)
                                    {
                                        free(jpeg_data);
                                        break;
                                    }
                                    if (send(client_fd, jpeg_data, jpeg_size, MSG_NOSIGNAL) <= 0)
                                    {
                                        free(jpeg_data);
                                        break;
                                    }

                                    frame_count++;
                                    if (frame_count % 30 == 0)
                                    {
                                        printf("Sent MJPEG frame %d (%u bytes)\n", frame_count, jpeg_size);
                                    }
                                }
                                else
                                {
                                    printf("Failed to read JPEG data: %zd/%u bytes\n", bytes_read, jpeg_size);
                                }
                                free(jpeg_data);
                            }
                            else
                            {
                                printf("Failed to allocate memory for JPEG data\n");
                            }
                        }
                        else
                        {
                            printf("Invalid JPEG size: %u bytes\n", jpeg_size);
                        }
                    }
                    else if (bytes_read == 0)
                    {
                        usleep(1000); // 1ms
                    }
                    else
                    {
                        printf("Read error: %s\n", strerror(errno));
                        usleep(1000);
                    }
                }

                close(jpeg_fd);
                printf("JPEG pipe closed\n");
            }
            else
            {
                printf("Failed to open JPEG stream pipe: %s\n", strerror(errno));
            }
            printf("=== MJPEG STREAM REQUEST ENDED ===\n");
        }
        else
        {
            char device_ip[64];
            get_device_ip(device_ip, sizeof(device_ip));

            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/html; charset=UTF-8\r\n"
                     "Connection: close\r\n\r\n"
                     "<!DOCTYPE html>"
                     "<html><head>"
                     "<title>CosmOS Transferd HTTP Output</title>"
                     "<style>"
                     "body{font-family:Arial;margin:20px;background:#000;color:white;}"
                     ".container{max-width:1000px;margin:0 auto;}"
                     "#videoElement{width:100%%;max-width:720px;border:2px solid #333;background:#000;}"
                     ".btn{padding:10px 15px;margin:5px;border:none;border-radius:5px;cursor:pointer;color:white;background:#007bff;}"
                     ".performance{background:#1a5f1a;padding:10px;border-radius:5px;margin:10px 0;}"
                     "</style>"
                     "</head><body>"
                     "<div class='container'>"
                     "<h1>CosmOS Transferd - MJPEG</h1>"
                     "<img id='videoElement' src='/api/video/stream' style='width:100%%;max-width:720px;border:2px solid #333;'>"
                     "<div>"
                     "</div>"
                     "<div id='status' class='performance'>Click Start MJPEG Stream for minimum latency</div>"
                     "<div class='performance'>"
                     "<strong>Mode:</strong> MJPEG with OpenCV encoding<br>"
                     "<strong>Resolution:</strong> 720x480 @ 30fps<br>"
                     "</div>"
                     "</div>"
                     "<script>"
                     "var video = document.getElementById('videoElement');"
                     "var status = document.getElementById('status');"
                     "function startStream() {"
                     "video.src = '/api/video/stream?' + Date.now();"
                     "status.innerHTML = 'MJPEG MODE ACTIVE! ~1ms latency';"
                     "}"
                     "setTimeout(startStream, 1000);"
                     "</script></body></html>");
            send(client_fd, response, strlen(response), 0);
        }
        close(client_fd);
    }
    return NULL;
}

int start_http_server(void)
{
    server_running = 1;
    webrtc_running = 1;

    // Start HTTP server thread
    if (pthread_create(&server_thread, NULL, http_thread, NULL) != 0)
    {
        log_error("Failed to create HTTP server thread");
        server_running = 0;
        webrtc_running = 0;
        return -1;
    }

    pthread_detach(server_thread);
    return 0;
}

void stop_http_server(void)
{
    server_running = 0;
    webrtc_running = 0;

    if (server_fd >= 0)
    {
        shutdown(server_fd, SHUT_RDWR);
        close(server_fd);
        server_fd = -1;
    }

    unlink("/tmp/h264_stream");
}