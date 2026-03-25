#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

struct app_config {
    char *default_wallpaper;
    // Additional monitor-specific settings can be added here
};

// Loads configuration from a file. Returns true if successful.
bool config_load(const char *path, struct app_config *config);

// Frees dynamically allocated configuration memory.
void config_free(struct app_config *config);

#endif
