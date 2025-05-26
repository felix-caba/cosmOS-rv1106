#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For isspace
#include <errno.h>

#include "../include/transferd.h"

// Helper function to trim leading/trailing whitespace
static char *trim_whitespace(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static source_type_t string_to_source_type(const char* str) {
    if (strcasecmp(str, "YOLO") == 0) return SOURCE_TYPE_YOLO;
    if (strcasecmp(str, "GPIO") == 0) return SOURCE_TYPE_GPIO;
    return SOURCE_TYPE_NONE;
}

const char* source_type_to_string(source_type_t type) {
    switch (type) {
        case SOURCE_TYPE_YOLO: return "YOLO";
        case SOURCE_TYPE_GPIO: return "GPIO";
        default: return "NONE";
    }
}



int load_config_from_file(transferd_config_t* config_out);
int save_config_to_file(const transferd_config_t* config_in);

int load_config_from_file(transferd_config_t* config_out) {
    if (!config_out) return -1;

    config_out->source_type = SOURCE_TYPE_NONE;

    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file) {
        log_error("Configuration file %s not found. Using default values.", CONFIG_FILE);
        return -1; 
    }

    char line[256];
    char key[128];
    char value_str[128];

    while (fgets(line, sizeof(line), file)) {
        char *trimmed_line = trim_whitespace(line);
        if (trimmed_line[0] == '#' || trimmed_line[0] == '\0') { 
            continue;
        }

        char *delimiter = strchr(trimmed_line, '=');
        if (delimiter) {
            *delimiter = '\0'; // Null-terminate the key
            strncpy(key, trim_whitespace(trimmed_line), sizeof(key) - 1);
            key[sizeof(key) -1] = '\0';

            strncpy(value_str, trim_whitespace(delimiter + 1), sizeof(value_str) - 1);
            value_str[sizeof(value_str) -1] = '\0';

            if (strcasecmp(key, "source_type") == 0) {
                config_out->source_type = string_to_source_type(value_str);
            } 
        }
    }
    fclose(file);
    log_message("Configuration loaded from %s", CONFIG_FILE);
    return 0;
}

int save_config_to_file(const transferd_config_t* config_in) {
    if (!config_in) return -1;

    FILE *file = fopen(CONFIG_FILE, "w");
    if (!file) {
        log_error("Failed to open configuration file %s for writing: %s", CONFIG_FILE, strerror(errno));
        return -1;
    }

    fprintf(file, "# Transferd Configuration File\n");
    fprintf(file, "source_type=%s\n", source_type_to_string(config_in->source_type));

    fclose(file);
    log_message("Configuration saved to %s", CONFIG_FILE);
    return 0;
}

extern transferd_config_t current_config; 

int transferd_load_config(const transferd_config_t* config_to_apply) {
    if (config_to_apply) {

        current_config = *config_to_apply;
    
        log_message("Transfer logic initialized with new config. Source: %s, Target: %s",
                    source_type_to_string(current_config.source_type));
        return 1; 
    } else {
        log_error("Failed to initialize transfer logic: null config provided to apply.");
   
        current_config.source_type = SOURCE_TYPE_NONE;
        return 0; 
    }
}