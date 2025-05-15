#ifndef SOURCE_H
#define SOURCE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t* data;
    size_t size;
    uint64_t timestamp;
} data_packet_t;

// Interfaz para fuentes de datos
typedef struct {
    int (*init)(void* config);
    int (*read_data)(data_packet_t* packet);
    void (*cleanup)(void);
} data_source_t;

#endif /* SOURCE_H */