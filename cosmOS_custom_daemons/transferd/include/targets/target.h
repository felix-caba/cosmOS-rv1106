#ifndef TARGET_H
#define TARGET_H

#include "../sources/source.h"

typedef struct {
    int (*init)(void* config);
    int (*send_data)(const data_packet_t* packet);
    int (*receive_data)(data_packet_t* packet);
    void (*cleanup)(void);
} data_target_t;

#endif /* TARGET_H */