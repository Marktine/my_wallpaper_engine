#include "daemon/plugin_manager.h"
#include "common/plugin.h"
#include <dirent.h>
#include <dlfcn.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct plugin_node {
  void *handle;
  const plugin_def_t *def;
  struct plugin_node *next;
} plugin_node_t;

static plugin_node_t *g_plugins = NULL;
static plugin_context_t g_plugin_ctx;
static pthread_t g_gtk_thread;

static void on_gtk_activate(GtkApplication *app, gpointer user_data) {
  (void)user_data;
  plugin_node_t *curr = g_plugins;
  while (curr) {
    if (curr->def->gtk_activate) {
      curr->def->gtk_activate(app, &g_plugin_ctx);
    }
    curr = curr->next;
  }
}

static void *gtk_thread_func(void *arg) {
  (void)arg;
  g_setenv("GDK_BACKEND", "wayland", TRUE);
  GtkApplication *app = gtk_application_new("com.my_wallpaper_engine.plugins",
                                            G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_gtk_activate), NULL);

  int status = g_application_run(G_APPLICATION(app), 0, NULL);
  (void)status;

  g_object_unref(app);
  return NULL;
}

void plugin_manager_init(const char *config_dir) {
  g_plugin_ctx.config_dir = config_dir;

  // CRITICAL: gtk4-layer-shell must be fully loaded (RTLD_NOW) and its symbols
  // made globally visible (RTLD_GLOBAL) BEFORE libwayland-client is initialized
  // by GTK inside any plugin. gtk4-layer-shell installs hooks into
  // wl_proxy_create at library constructor time — if libwayland is already
  // initialized when the library loads, those hooks are never installed, and
  // Hyprland treats the window as a regular xdg_toplevel tile instead of a
  // layer surface.
  void *layer_shell_preload =
      dlopen("libgtk4-layer-shell.so.0", RTLD_NOW | RTLD_GLOBAL);
  if (!layer_shell_preload) {
    fprintf(
        stderr,
        "[plugin-manager] WARNING: Failed to preload libgtk4-layer-shell: %s\n",
        dlerror());
    fprintf(stderr, "[plugin-manager] GTK plugins using layer-shell will NOT "
                    "work correctly.\n");
  } else {
    printf("[plugin-manager] Preloaded libgtk4-layer-shell (layer surface "
           "hooks installed)\n");
  }

  char plugins_dir[4096];
  snprintf(plugins_dir, sizeof(plugins_dir), "%s/plugins", config_dir);

  DIR *dir = opendir(plugins_dir);
  if (!dir) {
    // Plugins directory might not exist, which is fine
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    size_t len = strlen(entry->d_name);
    if (len > 3 && strcmp(entry->d_name + len - 3, ".so") == 0) {
      char path[8192];
      snprintf(path, sizeof(path), "%s/%s", plugins_dir, entry->d_name);

      void *handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
      if (!handle) {
        fprintf(stderr, "Failed to load plugin %s: %s\n", path, dlerror());
        continue;
      }

      get_plugin_info_fn get_info =
          (get_plugin_info_fn)dlsym(handle, "get_plugin_info");
      if (!get_info) {
        fprintf(stderr, "Plugin %s missing get_plugin_info: %s\n", path,
                dlerror());
        dlclose(handle);
        continue;
      }

      const plugin_def_t *def = get_info();
      if (!def || !def->init) {
        fprintf(stderr, "Plugin %s returned invalid definition\n", path);
        dlclose(handle);
        continue;
      }

      printf("Initializing plugin: %s (%s) version %s\n", def->name,
             def->description, def->version);
      if (def->init(&g_plugin_ctx) != 0) {
        fprintf(stderr, "Plugin %s failed to initialize\n", def->name);
        dlclose(handle);
        continue;
      }

      plugin_node_t *node = malloc(sizeof(plugin_node_t));
      node->handle = handle;
      node->def = def;
      node->next = g_plugins;
      g_plugins = node;
    }
  }
  closedir(dir);

  // Start the shared GTK thread to host all loaded plugins' UI
  pthread_create(&g_gtk_thread, NULL, gtk_thread_func, NULL);
}

void plugin_manager_cleanup(void) {
  plugin_node_t *curr = g_plugins;
  while (curr) {
    plugin_node_t *next = curr->next;
    if (curr->def->cleanup) {
      curr->def->cleanup();
    }
    dlclose(curr->handle);
    free(curr);
    curr = next;
  }
  g_plugins = NULL;
}
