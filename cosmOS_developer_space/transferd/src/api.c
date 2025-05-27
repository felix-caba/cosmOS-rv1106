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

