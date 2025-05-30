#include <stdio.h>
#include <stdarg.h>
#include "../include/transferd.h"

void log_message(const char *format, FILE *log_file, ...) {
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp == NULL) {
        perror("fopen (log file)");
        va_list args;
        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
        fprintf(stdout, "\n");
        return;
    }

    va_list args;
    va_start(args, format);
    vfprintf(log_fp, format, args);
    va_end(args);
    fprintf(log_fp, "\n");
    fclose(log_fp);
}

