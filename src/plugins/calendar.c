#include "common/plugin.h"
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static char *g_events[32] = {NULL};

static GtkWidget *main_stack;
static GtkWidget *tooltip_label;
static GtkEventController *main_motion_ctrl;
static int g_current_mon = 0;
static int g_current_year = 0;
static const char *g_month_names[] = {
    "JANUARY", "FEBRUARY", "MARCH",     "APRIL",   "MAY",      "JUNE",
    "JULY",    "AUGUST",   "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};

static void load_events() {
  for (int i = 0; i < 32; i++) {
    if (g_events[i]) {
      g_free(g_events[i]);
      g_events[i] = NULL;
    }
  }

  char path[1024];
  const char *home = getenv("HOME");
  if (!home)
    return;
  snprintf(path, sizeof(path), "%s/.cache/my_wallpaper_engine/events.txt",
           home);
  FILE *f = fopen(path, "r");
  if (!f)
    return;

  char line[256];
  while (fgets(line, sizeof(line), f)) {
    int day;
    char title[200];
    if (sscanf(line, "%d|%199[^\n]", &day, title) == 2) {
      if (day >= 1 && day <= 31) {
        if (g_events[day]) {
          char *old = g_events[day];
          g_events[day] = g_strdup_printf("%s\n- %s", old, title);
          g_free(old);
        } else {
          g_events[day] = g_strdup_printf("- %s", title);
        }
      }
    }
  }
  fclose(f);
}

static void on_widget_enter(GtkEventControllerMotion *controller, gdouble x,
                            gdouble y, gpointer user_data) {
  (void)controller;
  (void)x;
  (void)y;
  (void)user_data;
  load_events();
  gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "expanded");
}

static void on_widget_leave(GtkEventControllerMotion *controller,
                            gpointer user_data) {
  (void)controller;
  (void)user_data;
  gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "shrinked");
}

static void on_day_enter(GtkEventControllerMotion *controller, gdouble x,
                         gdouble y, gpointer user_data) {
  (void)controller;
  (void)x;
  (void)y;
  int day = GPOINTER_TO_INT(user_data);
  char buf[2048];

  const char *month_name = g_month_names[g_current_mon % 12];

  if (g_events[day]) {
    snprintf(buf, sizeof(buf),
             "<span size='small' alpha='70%%'>%s %d, %d</span>\n"
             "<span size='medium' weight='bold' color='#fceeb5'>EVENTS</span>\n"
             "<span color='#555555'>────────────────</span>\n"
             "<span font_family='Monospace' size='small'>%s</span>",
             month_name, day, g_current_year, g_events[day]);
  } else {
    snprintf(buf, sizeof(buf),
             "<span size='small' alpha='70%%'>%s %d, %d</span>\n"
             "<span size='medium' weight='bold' color='#fceeb5'>EVENTS</span>\n"
             "<span color='#555555'>────────────────</span>\n"
             "<span color='#888888' size='small' style='italic'>No events "
             "today</span>",
             month_name, day, g_current_year);
  }
  gtk_label_set_markup(GTK_LABEL(tooltip_label), buf);
}

static void on_relogin_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  (void)user_data;
  const char *home = getenv("HOME");
  if (!home)
    return;
  char cmd[1024];
  snprintf(cmd, sizeof(cmd),
           "%s/.config/my_wallpaper_engine/venv/bin/python "
           "%s/workspace/my_wallpaper_engine/src/scripts/gcal_fetcher.py",
           home, home);

  GError *err = NULL;
  g_spawn_command_line_async(cmd, &err);
  if (err) {
    fprintf(stderr, "Failed to launch relogin: %s\n", err->message);
    g_error_free(err);
  }
}

static void plugin_gtk_activate(void *app_ptr, plugin_context_t *ctx) {
  (void)ctx;
  GtkApplication *app = GTK_APPLICATION(app_ptr);
  GtkWidget *window = gtk_application_window_new(app);

  GtkCssProvider *provider = gtk_css_provider_new();
  const char *css = "window { background-color: transparent; }\n"
                    ".retro-cal {\n"
                    "   font-family: 'GohuFont uni14 Nerd Font', 'Terminess "
                    "Nerd Font', monospace;\n"
                    "   background-color: #fceeb5;\n"
                    "   color: #3f3f3f;\n"
                    "   border: 6px solid #2b2b2b;\n"
                    "   border-radius: 12px;\n"
                    "   padding: 8px;\n"
                    "   box-shadow: inset -4px -4px 0px rgba(0,0,0,0.1), 6px "
                    "6px 0px #111111;\n"
                    "}\n"
                    ".retro-day {\n"
                    "   background-color: #ffffff;\n"
                    "   border: 2px solid #2b2b2b;\n"
                    "   border-radius: 8px;\n"
                    "   padding: 4px;\n"
                    "   font-weight: bold;\n"
                    "   min-width: 36px;\n"
                    "   min-height: 36px;\n"
                    "   font-size: 14px;\n"
                    "   box-shadow: 2px 2px 0px #2b2b2b;\n"
                    "}\n"
                    ".retro-day:hover {\n"
                    "   background-color: #e0f0ff;\n"
                    "   box-shadow: inset 2px 2px 0px #2b2b2b;\n"
                    "}\n"
                    ".retro-day-small {\n"
                    "   background-color: #ffffff;\n"
                    "   border: 2px solid #2b2b2b;\n"
                    "   border-radius: 6px;\n"
                    "   padding: 4px 8px;\n"
                    "   font-weight: bold;\n"
                    "   font-size: 14px;\n"
                    "   box-shadow: 2px 2px 0px #2b2b2b;\n"
                    "}\n"
                    ".current-day {\n"
                    "   background-color: #ffcccc;\n"
                    "   color: #aa0000;\n"
                    "}\n"
                    ".retro-header {\n"
                    "   font-weight: bold;\n"
                    "   font-size: 14px;\n"
                    "   color: #555555;\n"
                    "   padding-bottom: 4px;\n"
                    "}\n"
                    ".retro-tooltip {\n"
                    "   margin-top: 15px;\n"
                    "   font-size: 14px;\n"
                    "   font-weight: bold;\n"
                    "   color: #ffffff;\n"
                    "   background-color: #2b2b2b;\n"
                    "   padding: 8px;\n"
                    "   border-radius: 4px;\n"
                    "}\n"
                    ".retro-button {\n"
                    "   background-color: #555555;\n"
                    "   color: #ffffff;\n"
                    "   border: 2px solid #2b2b2b;\n"
                    "   border-radius: 4px;\n"
                    "   padding: 4px 8px;\n"
                    "   font-weight: bold;\n"
                    "   font-size: 12px;\n"
                    "   box-shadow: 2px 2px 0px #2b2b2b;\n"
                    "}\n"
                    ".retro-button:hover {\n"
                    "   background-color: #777777;\n"
                    "}\n"
                    ".retro-button:active {\n"
                    "   box-shadow: inset 1px 1px 0px #2b2b2b;\n"
                    "}\n"
                    ".retro-button-small {\n"
                    "   background-color: transparent;\n"
                    "   border: 1px solid #2b2b2b;\n"
                    "   border-radius: 4px;\n"
                    "   padding: 2px 4px;\n"
                    "   font-size: 16px;\n"
                    "}\n"
                    ".retro-button-small:hover {\n"
                    "   background-color: rgba(0,0,0,0.1);\n"
                    "}";
  gtk_css_provider_load_from_string(provider, css);
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_layer_init_for_window(GTK_WINDOW(window));
  gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_BACKGROUND);
  gtk_layer_set_namespace(GTK_WINDOW(window), "wallpaper-calendar");
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
  gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 160);
  gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 40);
  gtk_layer_set_keyboard_mode(GTK_WINDOW(window),
                              GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

  main_stack = gtk_stack_new();
  // gtk_stack_set_transition_type(GTK_STACK(main_stack),
  //                               GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  // gtk_stack_set_transition_duration(GTK_STACK(main_stack), 100);
  gtk_stack_set_hhomogeneous(GTK_STACK(main_stack), FALSE);
  gtk_stack_set_vhomogeneous(GTK_STACK(main_stack), FALSE);

  // Box to wrap stack so we can attach motion controller to the whole area
  // easily
  GtkWidget *wrapper = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign(wrapper, GTK_ALIGN_END);
  gtk_widget_set_valign(wrapper, GTK_ALIGN_START);
  gtk_widget_add_css_class(wrapper, "retro-cal");
  gtk_box_append(GTK_BOX(wrapper), main_stack);

  main_motion_ctrl = gtk_event_controller_motion_new();
  g_signal_connect(main_motion_ctrl, "enter", G_CALLBACK(on_widget_enter),
                   NULL);
  g_signal_connect(main_motion_ctrl, "leave", G_CALLBACK(on_widget_leave),
                   NULL);
  gtk_widget_add_controller(wrapper, main_motion_ctrl);

  // Get current time
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int current_day = t->tm_mday;
  g_current_mon = t->tm_mon;
  g_current_year = t->tm_year + 1900;

  char current_day_str[32];
  strftime(current_day_str, sizeof(current_day_str), "%b\n%d", t);

  // --- Shrinked View ---
  GtkWidget *shrinked_label = gtk_label_new(current_day_str);
  gtk_label_set_justify(GTK_LABEL(shrinked_label), GTK_JUSTIFY_CENTER);
  gtk_widget_add_css_class(shrinked_label, "retro-day-small");
  gtk_widget_add_css_class(shrinked_label, "current-day");

  // Keep it centered in shrinked view
  GtkWidget *shrinked_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_halign(shrinked_box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(shrinked_box, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(shrinked_box), shrinked_label);
  gtk_stack_add_named(GTK_STACK(main_stack), shrinked_box, "shrinked");

  // --- Expanded View ---
  GtkWidget *expanded_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

  // Top Toolbar
  GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_margin_bottom(toolbar, 0);

  GtkWidget *relogin_btn = gtk_button_new_with_label("🔑");
  gtk_widget_add_css_class(relogin_btn, "retro-button-small");
  gtk_widget_set_tooltip_text(relogin_btn, "Re-Authorize Google Calendar");
  g_signal_connect(relogin_btn, "clicked", G_CALLBACK(on_relogin_clicked),
                   NULL);

  GtkWidget *toolbar_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(toolbar_spacer, TRUE);

  gtk_box_append(GTK_BOX(toolbar), toolbar_spacer);
  gtk_box_append(GTK_BOX(toolbar), relogin_btn);
  gtk_box_append(GTK_BOX(expanded_box), toolbar);

  // Month/Year header
  char header_str[64];
  strftime(header_str, sizeof(header_str), "  %B %Y  ", t);
  GtkWidget *header_label = gtk_label_new(header_str);
  gtk_widget_set_halign(header_label, GTK_ALIGN_CENTER);

  // Apply bold style
  char markup[128];
  snprintf(markup, sizeof(markup), "<span size='large' weight='bold'>%s</span>",
           header_str);
  gtk_label_set_markup(GTK_LABEL(header_label), markup);
  gtk_box_append(GTK_BOX(expanded_box), header_label);

  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 4);

  // Calculate starting day of week dynamically
  // tm_wday is 0-6 (Sun-Sat). Month starts at 1.
  // start_dow = (current_wday - (current_mday - 1) % 7 + 7) % 7
  int start_dow = (t->tm_wday - (t->tm_mday - 1) % 7 + 7) % 7;

  int days_in_month = 31;
  int mon = t->tm_mon;
  int year = t->tm_year + 1900;

  if (mon == 1) { // Feb
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
      days_in_month = 29;
    else
      days_in_month = 28;
  } else if (mon == 3 || mon == 5 || mon == 8 || mon == 10)
    days_in_month = 30;

  const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  for (int i = 0; i < 7; i++) {
    GtkWidget *h = gtk_label_new(days[i]);
    gtk_widget_add_css_class(h, "retro-header");
    gtk_grid_attach(GTK_GRID(grid), h, i, 0, 1, 1);
  }

  int row = 1;
  int col = start_dow;

  for (int i = 1; i <= days_in_month; i++) {
    char day_num[8];
    snprintf(day_num, sizeof(day_num), "%d", i);
    GtkWidget *day_label = gtk_label_new(day_num);
    gtk_widget_add_css_class(day_label, "retro-day");

    if (i == current_day) {
      gtk_widget_add_css_class(day_label, "current-day");
    }

    GtkEventController *day_motion = gtk_event_controller_motion_new();
    g_signal_connect(day_motion, "enter", G_CALLBACK(on_day_enter),
                     GINT_TO_POINTER(i));
    gtk_widget_add_controller(day_label, day_motion);

    gtk_grid_attach(GTK_GRID(grid), day_label, col, row, 1, 1);
    col++;
    if (col > 6) {
      col = 0;
      row++;
    }
  }

  tooltip_label = gtk_label_new("Hover over a day to see details");
  gtk_widget_add_css_class(tooltip_label, "retro-tooltip");
  gtk_widget_set_hexpand(tooltip_label, TRUE);
  gtk_widget_set_halign(tooltip_label, GTK_ALIGN_FILL);
  gtk_label_set_xalign(GTK_LABEL(tooltip_label), 0);
  gtk_label_set_justify(GTK_LABEL(tooltip_label), GTK_JUSTIFY_LEFT);

  GtkWidget *bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_append(GTK_BOX(bottom_box), tooltip_label);

  gtk_box_append(GTK_BOX(expanded_box), grid);
  gtk_box_append(GTK_BOX(expanded_box), bottom_box);

  gtk_stack_add_named(GTK_STACK(main_stack), expanded_box, "expanded");

  // Start shrinked
  gtk_stack_set_visible_child_name(GTK_STACK(main_stack), "shrinked");

  gtk_window_set_child(GTK_WINDOW(window), wrapper);
  gtk_window_present(GTK_WINDOW(window));
}

int plugin_init(plugin_context_t *ctx) {
  (void)ctx;
  return 0;
}

void plugin_cleanup(void) {}

static const plugin_def_t def = {.name = "calendar",
                                 .description =
                                     "A retro hovering calendar plugin",
                                 .version = "1.0",
                                 .init = plugin_init,
                                 .gtk_activate = plugin_gtk_activate,
                                 .cleanup = plugin_cleanup};

const plugin_def_t *get_plugin_info(void) { return &def; }
