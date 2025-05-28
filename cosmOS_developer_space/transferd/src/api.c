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
        if (client_fd < 0)
            continue;

        char buffer[1024] = {0};
        read(client_fd, buffer, 1024);

        char response[8192]; // Increased buffer size for HTML

        if (strstr(buffer, "GET /api/detections"))
        {
            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/json\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Connection: close\r\n\r\n"
                     "%s",
                     latest_message);
        }
        else
        {
            // Main page with RTSP video player and detection info
            char device_ip[64];
            get_device_ip(device_ip, sizeof(device_ip));

            snprintf(response, sizeof(response),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/html; charset=UTF-8\r\n"
                     "Connection: close\r\n\r\n"
                     "<!DOCTYPE html>"
                     "<html><head>"
                     "<title>YOLO Live Detection System</title>"
                     "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                     "<meta charset='UTF-8'>"
                     "<style>"
                     "body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5;}"
                     ".container{max-width:1200px;margin:0 auto;}"
                     ".header{text-align:center;margin-bottom:30px;}"
                     ".content{display:flex;gap:20px;flex-wrap:wrap;}"
                     ".video-section{flex:1;min-width:400px;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
                     ".info-section{flex:0 0 300px;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
                     ".detection-box{background:#28a745;color:white;padding:15px;border-radius:8px;margin:10px 0;text-align:center;}"
                     ".network-info{background:#f8f9fa;padding:15px;border-radius:8px;border:1px solid #dee2e6;}"
                     ".status{padding:10px;margin:5px 0;border-radius:5px;}"
                     ".online{background:#d4edda;color:#155724;border:1px solid #c3e6cb;}"
                     ".offline{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}"
                     "h1{color:#333;margin:0;}"
                     "h2{color:#555;margin-top:0;}"
                     ".btn{padding:10px 15px;margin:5px;background:#007bff;color:white;border:none;border-radius:5px;cursor:pointer;text-decoration:none;display:inline-block;}"
                     ".btn:hover{background:#0056b3;}"
                     ".video-placeholder{background:#000;color:#fff;padding:50px;text-align:center;border-radius:8px;}"
                     "</style>"
                     "</head><body>"
                     "<div class='container'>"
                     "<div class='header'>"
                     "<h1>YOLO Live Detection System</h1>"
                     "<p>Real-time object detection with RTSP streaming</p>"
                     "</div>"
                     "<div class='content'>"
                     "<div class='video-section'>"
                     "<h2>Live Video Stream</h2>"
                     "<div class='video-placeholder'>"
                     "<h3>RTSP Stream Available</h3>"
                     "<p><strong>RTSP URL:</strong> <code>rtsp://%s:554/live/0</code></p>"
                     "<p>Browsers don't support RTSP directly. Use VLC or another RTSP client.</p>"
                     "<div style='margin-top:15px;'>"
                     "<a href='vlc://rtsp://%s:554/live/0' class='btn'>Open in VLC</a>"
                     "<button onclick='copyRtspUrl()' class='btn'>Copy RTSP URL</button>"
                     "</div>"
                     "</div>"
                     "</div>"
                     "<div class='info-section'>"
                     "<h2>Detection Status</h2>"
                     "<div id='detection-status' class='detection-box'>Waiting for detections...</div>"
                     "<div id='connection-status' class='status offline'>Connecting...</div>"
                     "<h3>Network Information</h3>"
                     "<div class='network-info'>"
                     "<p><strong>Device IP:</strong> %s</p>"
                     "<p><strong>HTTP API:</strong> <a href='http://%s:8080' target='_blank'>http://%s:8080</a></p>"
                     "<p><strong>RTSP Stream:</strong> <code>rtsp://%s:554/live/0</code></p>"
                     "<p><strong>Detection API:</strong> <a href='/api/detections' target='_blank'>/api/detections</a></p>"
                     "</div>"
                     "<h3>Instructions</h3>"
                     "<div class='network-info'>"
                     "<p>• Copy RTSP URL and open in VLC Media Player</p>"
                     "<p>• Detection data updates every 500ms</p>"
                     "<p>• Green status = detections active</p>"
                     "<p>• Red status = no connection</p>"
                     "</div>"
                     "</div>"
                     "</div>"
                     "</div>"
                     "<script>"
                     "let lastUpdateTime = 0;"
                     "function updateDetections(){"
                     "fetch('/api/detections')"
                     ".then(r=>r.json())"
                     ".then(d=>{"
                     "const detDiv = document.getElementById('detection-status');"
                     "const statusDiv = document.getElementById('connection-status');"
                     "if(d.object && d.timestamp){"
                     "const timeStr = new Date(d.timestamp*1000).toLocaleTimeString();"
                     "detDiv.innerHTML='<strong>'+d.object.toUpperCase()+'</strong><br><small>Detected at '+timeStr+'</small>';"
                     "detDiv.style.background='#28a745';"
                     "statusDiv.innerHTML='Connected - Live detections';"
                     "statusDiv.className='status online';"
                     "lastUpdateTime = Date.now();"
                     "}else{"
                     "detDiv.innerHTML='<strong>SCANNING...</strong><br><small>No objects detected</small>';"
                     "detDiv.style.background='#6c757d';"
                     "}"
                     "})"
                     ".catch(e=>{"
                     "const statusDiv = document.getElementById('connection-status');"
                     "statusDiv.innerHTML='Connection Error';"
                     "statusDiv.className='status offline';"
                     "console.error('Detection API error:',e);"
                     "});"
                     "}"
                     "function copyRtspUrl(){"
                     "const url = 'rtsp://%s:554/live/0';"
                     "navigator.clipboard.writeText(url).then(()=>{"
                     "alert('RTSP URL copied to clipboard!');"
                     "}).catch(()=>{"
                     "prompt('Copy this RTSP URL:', url);"
                     "});"
                     "}"
                     "setInterval(updateDetections, 500);"
                     "updateDetections();"
                     "setInterval(()=>{"
                     "if(Date.now() - lastUpdateTime > 5000){"
                     "const statusDiv = document.getElementById('connection-status');"
                     "statusDiv.innerHTML='No recent updates';"
                     "statusDiv.className='status offline';"
                     "}"
                     "}, 2000);"
                     "</script>"
                     "</body></html>",
                     device_ip, device_ip, device_ip, device_ip, device_ip, device_ip, device_ip);

            send(client_fd, response, strlen(response), 0);
            close(client_fd);
        }
        return NULL;
    } 
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