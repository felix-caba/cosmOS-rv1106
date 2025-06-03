#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "../include/transferd.h"

#define LM75_I2C_ADDR 0x48
#define LM75_REG_TEMP 0x00
#define LM75_REG_CONF 0x01

typedef struct {
    int fd; // descriptor
    bool initialized;
    pthread_t thread;
    bool thread_running;
    float last_temperature;
    pthread_mutex_t temp_mutex; // mutex
} lm75_t;

static lm75_t global_sensor = {
    .fd = -1,
    .initialized = false,
    .thread_running = false,
    .last_temperature = -999.0f,
    .temp_mutex = PTHREAD_MUTEX_INITIALIZER
};

int lm75_init(lm75_t *sensor, const char *i2c_device)
{
    sensor->fd = open(i2c_device, O_RDWR);
    if (sensor->fd < 0) {
        log_message("LM75: Error opening I2C device %s", TEMP_LOG_FILE, i2c_device);
        return -1;
    }

    if (ioctl(sensor->fd, I2C_SLAVE, LM75_I2C_ADDR) < 0) {
        log_message("LM75: Error setting I2C slave address 0x%02X", TEMP_LOG_FILE, LM75_I2C_ADDR);
        close(sensor->fd);
        return -1;
    }

    // Configure LM75 - normal operation
    uint8_t config_data[2] = {LM75_REG_CONF, 0x00};
    if (write(sensor->fd, config_data, 2) != 2) {
        log_message("LM75: Failed to configure sensor", TEMP_LOG_FILE);
        close(sensor->fd);
        return -1;
    }

    sensor->initialized = true;
    log_message("LM75: Temperature sensor initialized on %s", TEMP_LOG_FILE, i2c_device);
    return 0;
}

float lm75_read_temperature(lm75_t *sensor)
{
    if (!sensor->initialized) {
        return -999.0f;
    }

    // escribo el reg
    uint8_t reg = LM75_REG_TEMP;
    if (write(sensor->fd, &reg, 1) != 1) {
        log_message("LM75: Failed to write register address", TEMP_LOG_FILE);
        return -999.0f;
    }

    // lee 2 bytes el sensor pero luego sobran 7 bites
    // Byte 0: [D8 D7 D6 D5 D4 D3 D2 D1] (MSB) most signif
    // Byte 1: [D0  0  0  0  0  0  0  0] (LSB) least signi
    uint8_t temp_data[2];
    if (read(sensor->fd, temp_data, 2) != 2) {
        log_message("LM75: Failed to read temperature data", TEMP_LOG_FILE);
        return -999.0f;
    }

    int16_t raw_temp = (temp_data[0] << 8) | temp_data[1];
    
    // me cargo los bites sobrantes
    raw_temp >>= 7;
    
    float temperature = raw_temp * 0.5f;
    
    return temperature;
}

void *temp_sensor_thread(void *arg)
{
    lm75_t *sensor = (lm75_t *)arg;
    float previous_temp = -999.0f;
    
    log_message("LM75: Temperature sensor thread started", TEMP_LOG_FILE);
    
    while (sensor->thread_running) {
        float current_temp = lm75_read_temperature(sensor);
        
        if (current_temp != -999.0f) {
            // solo si cambia mucho
            if (previous_temp == -999.0f || 
                fabs(current_temp - previous_temp) >= 0.5f) {
                
                pthread_mutex_lock(&sensor->temp_mutex);
                sensor->last_temperature = current_temp;
                pthread_mutex_unlock(&sensor->temp_mutex);
                
                char temp_string[32];
                snprintf(temp_string, sizeof(temp_string), "%.1fC", current_temp);
                
                log_message("LM75: Temperature reading: %.1f°C", TEMP_LOG_FILE, current_temp);
                previous_temp = current_temp;
            }
        } else {
            log_message("LM75: Failed to read temperature", TEMP_LOG_FILE);
        }
        
        sleep(2);
    }
    
    log_message("LM75: Temperature sensor thread stopped", TEMP_LOG_FILE);
    return NULL;
}

void lm75_cleanup(lm75_t *sensor)
{
    if (sensor->thread_running) {
        sensor->thread_running = false;
        pthread_join(sensor->thread, NULL);
        log_message("LM75: Temperature thread joined", TEMP_LOG_FILE);
    }
    
    if (sensor->initialized && sensor->fd >= 0) {
        close(sensor->fd);
        sensor->fd = -1;
        sensor->initialized = false;
        log_message("LM75: Temperature sensor stopped", TEMP_LOG_FILE);
    }
}

int start_temp_sensor(void)
{
    if (lm75_init(&global_sensor, "/dev/i2c-3") == 0) {
        global_sensor.thread_running = true;
        
        if (pthread_create(&global_sensor.thread, NULL, temp_sensor_thread, &global_sensor) != 0) {
            log_message("LM75: Failed to create temperature thread: %s", TEMP_LOG_FILE, strerror(errno));
            lm75_cleanup(&global_sensor);
            return -1;
        }
        
        log_message("LM75: Temperature sensor started successfully with thread", TEMP_LOG_FILE);
        return 0;
    } else {
        log_message("LM75: Failed to start temperature sensor", TEMP_LOG_FILE);
        return -1;
    }
}

void stop_temp_sensor(void)
{
    lm75_cleanup(&global_sensor);
}

char* get_temp_reading(void)
{
    static char temp_string[32];
    
    if (!global_sensor.initialized) {
        snprintf(temp_string, sizeof(temp_string), "TEMP ERROR");
        return temp_string;
    }

    pthread_mutex_lock(&global_sensor.temp_mutex);
    float last_temp = global_sensor.last_temperature;
    pthread_mutex_unlock(&global_sensor.temp_mutex);
    
    if (last_temp != -999.0f) {
        snprintf(temp_string, sizeof(temp_string), "%.1fC", last_temp);
    } else {
        snprintf(temp_string, sizeof(temp_string), "TEMP WAIT");
    }
    
    return temp_string;
}
