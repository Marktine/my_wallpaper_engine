#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include "common/plugin.h"

static void plugin_gtk_activate(void *app_ptr, plugin_context_t *ctx) {
    (void)ctx;
    GtkApplication *app = GTK_APPLICATION(app_ptr);
    GtkWidget *window = gtk_application_window_new(app);
    
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 20);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 20);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, "label { color: #00ffff; font-family: sans-serif; font-weight: bold; font-size: 24px; background: rgba(0,0,0,0.6); padding: 15px; border-radius: 12px; }");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *label = gtk_label_new("Plugin Widget (GTK4)");
    gtk_window_set_child(GTK_WINDOW(window), label);
    
    gtk_window_present(GTK_WINDOW(window));
}

int plugin_init(plugin_context_t *ctx) {
    (void)ctx;
    return 0;
}

void plugin_cleanup(void) {
    // In production we would signal the GTK layer to close safely.
}

static const plugin_def_t def = {
    .name = "sample_gtk4_widget",
    .description = "A sample GTK4 widget overlay using gtk4-layer-shell and CSS",
    .version = "1.0",
    .init = plugin_init,
    .gtk_activate = plugin_gtk_activate,
    .cleanup = plugin_cleanup
};

const plugin_def_t* get_plugin_info(void) {
    return &def;
}
