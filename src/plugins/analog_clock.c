#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <time.h>
#include "common/plugin.h"

static gboolean update_clock(GtkWidget *label) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    
    // Format: HH:MM:SS AM/PM
    strftime(time_str, sizeof(time_str), "%I:%M:%S %p", t);
    
    gtk_label_set_text(GTK_LABEL(label), time_str);
    return TRUE; // Continue timeout
}

static void plugin_gtk_activate(void *app_ptr, plugin_context_t *ctx) {
    (void)ctx;
    GtkApplication *app = GTK_APPLICATION(app_ptr);
    GtkWidget *window = gtk_application_window_new(app);
    
    // Transparent window background with crisp text rendering
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css = 
        "window { background-color: transparent; }\n"
        "label.pixel-clock {\n"
        "   font-family: 'GohuFont uni14 Nerd Font', 'Terminess Nerd Font', monospace;\n"
        "   font-size: 64px;\n"
        "   color: #e0e0e0;\n"
        "   text-shadow: 4px 4px 0px #0a0a0a, -2px -2px 0px #0a0a0a, 2px -2px 0px #0a0a0a, -2px 2px 0px #0a0a0a;\n"
        "   font-weight: bold;\n"
        "   padding: 10px;\n"
        "}";
    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    // Layer shell setup: Make it a background widget at top right
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_BACKGROUND);
    gtk_layer_set_namespace(GTK_WINDOW(window), "wallpaper-clock");
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 40);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 40);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    // Label setup
    GtkWidget *label = gtk_label_new("");
    gtk_widget_add_css_class(label, "pixel-clock");
    
    // Set initial text
    update_clock(label);
    
    gtk_window_set_child(GTK_WINDOW(window), label);
    
    // Update every second (1000ms)
    g_timeout_add(1000, (GSourceFunc)update_clock, label);
    
    gtk_window_present(GTK_WINDOW(window));
}

int plugin_init(plugin_context_t *ctx) {
    (void)ctx;
    return 0;
}

void plugin_cleanup(void) {
    // Cleanup is handled by the main daemon tearing down the UI thread
}

static const plugin_def_t def = {
    .name = "digital_clock",
    .description = "A classic digital pixel-styled clock using GTK4 Label and Layer Shell",
    .version = "1.1",
    .init = plugin_init,
    .gtk_activate = plugin_gtk_activate,
    .cleanup = plugin_cleanup
};

const plugin_def_t* get_plugin_info(void) {
    return &def;
}
