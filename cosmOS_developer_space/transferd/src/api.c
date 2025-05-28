#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "../include/transferd.h"

static int server_fd = -1; // pipe read fd
static pthread_t server_thread; // hilo del servidor 
static int server_running = 0;
static char latest_message[256] = "";

void update_detection(const char* object_name) { // actualiza el objeto detectado latest message.
    time_t now = time(NULL);
    snprintf(latest_message, sizeof(latest_message), 
             "{\"object\":\"%s\",\"timestamp\":%ld}", object_name, now);
}

static void* http_thread(void* arg){
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 3);

    log_message("HTTP Server started on 8080");

     while (server_running) {
        int client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_fd < 0) continue;
        
        char buffer[1024] = {0};
        read(client_fd, buffer, 1024);
        
        char response[2048];
        if (strstr(buffer, "GET /api/detections")) {
            snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Connection: close\r\n\r\n"
                "%s", latest_message);
        } else {
            snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n\r\n"
                "<html><body><h1>YOLO API</h1>"
                "<script>setInterval(()=>fetch('/api/detections')"
                ".then(r=>r.json()).then(d=>document.body.innerHTML="
                "'<h1>Latest: '+d.object+'</h1>'),1000)</script></body></html>");
        }
        
        send(client_fd, response, strlen(response), 0);
        close(client_fd);
    }
    return NULL;
}


int start_http_server(void) {
    server_running = 1;
    if (pthread_create(&server_thread, NULL, http_thread, NULL) != 0) {
        log_error("Failed to create HTTP server thread");
        return -1;
    }
    return 0;
}

void stop_http_server(void) {
    server_running = 0;
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    pthread_join(server_thread, NULL);
}