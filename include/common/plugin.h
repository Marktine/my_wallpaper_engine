#ifndef PLUGIN_H
#define PLUGIN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Context passed to the plugin during initialization
typedef struct plugin_context {
    const char *config_dir;
    // We can add callbacks here allowing the plugin to request things from the daemon
    // e.g. void (*set_wallpaper)(const char *path);
} plugin_context_t;

typedef struct plugin_def {
    const char *name;
    const char *description;
    const char *version;
    
    // Called once when the plugin is loaded.
    // Returns 0 on success, non-zero on failure.
    int (*init)(plugin_context_t *ctx);
    
    // Optional: called on the dedicated shared GTK application thread when it activates.
    // `app` is a pointer to the single shared GtkApplication.
    void (*gtk_activate)(void *app, plugin_context_t *ctx);
    
    // Called before unloading the plugin.
    void (*cleanup)(void);
} plugin_def_t;

// The function every plugin MUST export
typedef const plugin_def_t* (*get_plugin_info_fn)(void);

#ifdef __cplusplus
}
#endif

#endif // PLUGIN_H
