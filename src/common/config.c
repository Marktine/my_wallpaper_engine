#define _POSIX_C_SOURCE 200809L
#include "common/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

bool config_load(const char *path, struct app_config *config) {
    memset(config, 0, sizeof(struct app_config));
    FILE *file = fopen(path, "r");
    if (!file) {
        return false;
    }

    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        fclose(file);
        return false;
    }

    yaml_parser_set_input_file(&parser, file);

    char *current_key = NULL;

    do {
        if (!yaml_parser_parse(&parser, &event)) {
            break;
        }

        // Basic flat key-value parser state-machine
        if (event.type == YAML_SCALAR_EVENT) {
            char *val = (char *)event.data.scalar.value;
            if (current_key) {
                if (strcmp(current_key, "default_wallpaper") == 0) {
                    config->default_wallpaper = strdup(val);
                }
                free(current_key);
                current_key = NULL;
            } else {
                current_key = strdup(val);
            }
        } else if (event.type != YAML_ALIAS_EVENT && event.type != YAML_DOCUMENT_START_EVENT) {
            if (current_key) {
                free(current_key);
                current_key = NULL;
            }
        }

        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;

    } while (1);

    if (current_key) free(current_key);
    yaml_parser_delete(&parser);
    fclose(file);
    return true;
}

void config_free(struct app_config *config) {
    if (config->default_wallpaper) {
        free(config->default_wallpaper);
    }
    memset(config, 0, sizeof(struct app_config));
}
