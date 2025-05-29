#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include "../include/transferd.h"
#include <stdio.h>

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

// Function to get device IP
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

            // Skip loopback and look for real network interface
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

    log_message("HTTP Server started on 8080");

    while (server_running)
    {
        int client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_fd < 0) continue;

        char buffer[1024] = {0};
        read(client_fd, buffer, 1024);

        char response[8192];

        if (strstr(buffer, "GET /api/detections"))
        {
            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/json\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Connection: close\r\n\r\n"
                     "%s", latest_message);
            send(client_fd, response, strlen(response), 0);
        }
        else if (strstr(buffer, "GET /stream.mjpeg"))
        {
            // MJPEG streaming for ultra-low latency
            char header[] = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
                "Cache-Control: no-cache\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: close\r\n\r\n";
            
            send(client_fd, header, strlen(header), 0);
            
            // Stream frames at 15 FPS for low latency
            for (int i = 0; i < 900 && server_running; i++) { // 1 minute max
                FILE *jpg = fopen("/tmp/frame.jpg", "rb");
                if (jpg) {
                    fseek(jpg, 0, SEEK_END);
                    long jpg_size = ftell(jpg);
                    if (jpg_size > 0 && jpg_size < 200000) { // Max 200KB safety
                        fseek(jpg, 0, SEEK_SET);
                        
                        char frame_header[256];
                        snprintf(frame_header, sizeof(frame_header),
                            "--frame\r\n"
                            "Content-Type: image/jpeg\r\n"
                            "Content-Length: %ld\r\n\r\n", jpg_size);
                        
                        if (send(client_fd, frame_header, strlen(frame_header), 0) <= 0) {
                            fclose(jpg);
                            break;
                        }
                        
                        char img_buffer[4096];
                        size_t bytes;
                        while ((bytes = fread(img_buffer, 1, sizeof(img_buffer), jpg)) > 0) {
                            if (send(client_fd, img_buffer, bytes, 0) <= 0) break;
                        }
                        send(client_fd, "\r\n", 2, 0);
                    }
                    fclose(jpg);
                }
                usleep(66000); // ~15 FPS (66ms delay)
            }
        }
        else
        {
            // Main HTML page with MJPEG video
            char device_ip[64];
            get_device_ip(device_ip, sizeof(device_ip));

            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/html; charset=UTF-8\r\n"
                     "Connection: close\r\n\r\n"
                     "<!DOCTYPE html>"
                     "<html><head>"
                     "<title>YOLO Live Detection - MJPEG Stream</title>"
                     "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                     "<style>"
                     "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}"
                     ".container{max-width:1000px;margin:0 auto;}"
                     ".content{display:flex;gap:20px;flex-wrap:wrap;}"
                     ".video-section{flex:1;min-width:400px;background:white;padding:20px;border-radius:10px;}"
                     ".info-section{flex:0 0 300px;background:white;padding:20px;border-radius:10px;}"
                     ".detection-box{background:#28a745;color:white;padding:15px;border-radius:8px;margin:10px 0;text-align:center;}"
                     ".network-info{background:#f8f9fa;padding:15px;border-radius:8px;}"
                     ".status{padding:10px;margin:5px 0;border-radius:5px;}"
                     ".online{background:#d4edda;color:#155724;}"
                     ".offline{background:#f8d7da;color:#721c24;}"
                     "#videoStream{max-width:100%%;border:2px solid #333;border-radius:8px;}"
                     "</style>"
                     "</head><body>"
                     "<div class='container'>"
                     "<h1>YOLO Live Detection - MJPEG Stream</h1>"
                     "<div class='content'>"
                     "<div class='video-section'>"
                     "<h2>Live Video Stream (MJPEG)</h2>"
                     "<img id='videoStream' src='/stream.mjpeg' alt='Live MJPEG Stream'>"
                     "<p><strong>Stream Type:</strong> Motion JPEG (MJPEG)</p>"
                     "<p><strong>Latency:</strong> ~200-500ms</p>"
                     "</div>"
                     "<div class='info-section'>"
                     "<h2>Detection Status</h2>"
                     "<div id='detection-status' class='detection-box'>Waiting...</div>"
                     "<div id='connection-status' class='status offline'>Connecting...</div>"
                     "<h3>Network Info</h3>"
                     "<div class='network-info'>"
                     "<p><strong>Device IP:</strong> %s</p>"
                     "<p><strong>HTTP API:</strong> http://%s:8080</p>"
                     "<p><strong>MJPEG Stream:</strong> /stream.mjpeg</p>"
                     "</div>"
                     "</div>"
                     "</div>"
                     "</div>"
                     "<script>"
                     "function updateDetections(){"
                     "fetch('/api/detections')"
                     ".then(r=>r.json())"
                     ".then(d=>{"
                     "const detDiv = document.getElementById('detection-status');"
                     "const statusDiv = document.getElementById('connection-status');"
                     "if(d.object && d.timestamp){"
                     "const timeStr = new Date(d.timestamp*1000).toLocaleTimeString();"
                     "detDiv.innerHTML='<strong>'+d.object.toUpperCase()+'</strong><br><small>'+timeStr+'</small>';"
                     "statusDiv.innerHTML='Connected - Live detections';"
                     "statusDiv.className='status online';"
                     "}else{"
                     "detDiv.innerHTML='<strong>SCANNING...</strong><br><small>No objects detected</small>';"
                     "statusDiv.className='status online';"
                     "}"
                     "})"
                     ".catch(e=>{"
                     "document.getElementById('connection-status').innerHTML='Connection Error';"
                     "document.getElementById('connection-status').className='status offline';"
                     "});"
                     "}"
                     "setInterval(updateDetections, 200);" // Faster updates - 200ms
                     "updateDetections();"
                     "</script></body></html>",
                     device_ip, device_ip);
            
            send(client_fd, response, strlen(response), 0);
        }
        
        close(client_fd);
    }
    return NULL; // THIS WAS MISSING - CRITICAL FIX!
}

    int start_http_server(void)
    {
        server_running = 1;

        if (pthread_create(&server_thread, NULL, http_thread, NULL) != 0)
        {
            log_error("Failed to create HTTP server thread");
            server_running = 0;
            return -1;
        }

        pthread_detach(server_thread);
        return 0;
    }

    void stop_http_server(void)
    {
        server_running = 0;
        if (server_fd >= 0)
        {
            shutdown(server_fd, SHUT_RDWR);
            close(server_fd);
            server_fd = -1;
        }
    }