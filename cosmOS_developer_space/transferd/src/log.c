#include <stdio.h>
#include <stdarg.h>
#include "../include/transferd.h"

void log_message(const char *format, char *log_file, ...) {
    FILE *log_fp = fopen(log_file, "a");
    if (log_fp == NULL) {
        perror("fopen (log file)");
        va_list args;
        va_start(args, log_file);
        vfprintf(stdout, format, args);
        va_end(args);
        fprintf(stdout, "\n");
        return;
    }

    va_list args;
    va_start(args, log_file); 
    vfprintf(log_fp, format, args);
    va_end(args);
    fprintf(log_fp, "\n");
    fclose(log_fp);
}