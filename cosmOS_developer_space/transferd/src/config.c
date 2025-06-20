#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> // For isspace
#include <errno.h>

#include "../include/transferd.h"

// Helper function to trim leading/trailing whitespace
static char *trim_whitespace(char *str)
{
    char *end;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return str;
}

static source_type_t string_to_source_type(const char *str)
{
    if (strcasecmp(str, "YOLO") == 0)
        return SOURCE_TYPE_YOLO;
    if (strcasecmp(str, "I2C_TEMP") == 0)
        return SOURCE_TYPE_I2C_TEMP;
    return SOURCE_TYPE_NONE;
}

const char *output_type_to_string(output_type_t type)
{
    switch (type)
    {
    case OUTPUT_TYPE_SCREEN:
        return "SCREEN";
    case OUTPUT_TYPE_HTTP:
        return "HTTP";
    default:
        return "UNKNOWN";
    }
}

const char *source_type_to_string(source_type_t type)
{
    switch (type)
    {
    case SOURCE_TYPE_YOLO:
        return "YOLO";
    case SOURCE_TYPE_I2C_TEMP:
        return "I2C_TEMP";
    default:
        return "NONE";
    }
}

static output_type_t string_to_output_type(const char *str)
{
    if (strcasecmp(str, "SCREEN") == 0)
        return OUTPUT_TYPE_SCREEN;
    if (strcasecmp(str, "HTTP") == 0)
        return OUTPUT_TYPE_HTTP;
    return OUTPUT_TYPE_HTTP;
}



void load_config_from_file(transferd_config_t *config_out);
int save_config_to_file(const transferd_config_t *config_in);

void load_config_from_file(transferd_config_t *config_out)
{
    if (!config_out)
        return;

    config_out->source_type = SOURCE_TYPE_YOLO; 
    config_out->output_type = OUTPUT_TYPE_SCREEN;

    FILE *file = fopen(CONFIG_FILE, "r");
    if (!file)
    {
        log_message("Failed to open configuration file %s: %s", LOG_FILE, CONFIG_FILE, strerror(errno));
        return;
    }

    char line[256];
    char key[128];
    char value_str[128];

    while (fgets(line, sizeof(line), file))
    {
        char *trimmed_line = trim_whitespace(line);
        if (trimmed_line[0] == '#' || trimmed_line[0] == '\0')
        {
            continue;
        }

        char *delimiter = strchr(trimmed_line, '=');
        if (delimiter)
        {
            *delimiter = '\0';
            strncpy(key, trim_whitespace(trimmed_line), sizeof(key) - 1);
            key[sizeof(key) - 1] = '\0';

            strncpy(value_str, trim_whitespace(delimiter + 1), sizeof(value_str) - 1);
            value_str[sizeof(value_str) - 1] = '\0';

            if (strcasecmp(key, "source_type") == 0)
            {
                config_out->source_type = string_to_source_type(value_str);
            }

            if (strcasecmp(key, "output_type") == 0)
            {
                config_out->output_type = string_to_output_type(value_str);
            }
        }
    }
    fclose(file);

    return;
}

int save_config_to_file(const transferd_config_t *config_in)
{
    if (!config_in)
        return -1;

    FILE *file = fopen(CONFIG_FILE, "w");
    if (!file)
    {
        log_message("Failed to open configuration file %s for writing: %s", LOG_FILE, strerror(errno), CONFIG_FILE);
        return -1;
    }

    fprintf(file, "# Transferd Configuration File\n");
    fprintf(file, "source_type=%s\n", source_type_to_string(config_in->source_type));
    fprintf(file, "output_type=%s\n", output_type_to_string(config_in->output_type));

    fclose(file);
    log_message("Configuration saved to %s", LOG_FILE, CONFIG_FILE);
    return 0;
}

extern transferd_config_t current_config;

int transferd_load_config(const transferd_config_t *config_to_apply)
{
    if (config_to_apply)
    {

        current_config = *config_to_apply;

        log_message("Transfer logic initialized with new config. Source: %s, Target: %s", LOG_FILE,
                    source_type_to_string(current_config.source_type),
                    output_type_to_string(current_config.output_type));

        return 1;
    }
    else
    {
        log_message("Failed to initialize transfer logic: null config provided to apply.", LOG_FILE);

        current_config.source_type = SOURCE_TYPE_NONE;
        return 0;
    }
}

