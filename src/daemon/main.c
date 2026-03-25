#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/eventfd.h>
#include "daemon/render.h"
#include "daemon/mpv_render.h"
#include "daemon/server_ipc.h"
#include "daemon/hyprland_ipc.h"
#include "common/config.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct render_state g_render_state;
GLuint g_texture = 0;
bool g_gl_initialized = false;
bool g_needs_redraw = false;
bool g_video_mode = false;
bool g_wallpaper_changed = false;
const char *g_wallpaper_path = NULL;
int g_mpv_wakeup_fd = -1;

struct app_config g_config;
char g_config_path[1024] = {0};

struct wl_display *display = NULL;
struct wl_registry *registry = NULL;
struct wl_compositor *compositor = NULL;
struct wl_shm *shm = NULL;
struct zwlr_layer_shell_v1 *layer_shell = NULL;

struct output {
    uint32_t name;
    struct wl_output *wl_output;
    struct wl_surface *wl_surface;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    int width, height;
    struct output *next;
};

struct output *outputs = NULL;

bool is_video(const char *path) {
    if (!path) return false;
    size_t len = strlen(path);
    if (len < 4) return false;
    const char *ext = path + len - 4;
    return (strcasecmp(ext, ".mp4") == 0 || strcasecmp(ext, ".mkv") == 0 || strcasecmp(ext, ".gif") == 0 || strstr(ext, "webm") != NULL);
}

void on_mpv_update_cb(void) {
    uint64_t u = 1;
    write(g_mpv_wakeup_fd, &u, sizeof(uint64_t));
}

static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height) {
    struct output *out = data;
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    
    if (width == 0 || height == 0) return;
    out->width = width;
    out->height = height;

    if (!out->egl_window) {
        out->egl_window = wl_egl_window_create(out->wl_surface, width, height);
        out->egl_surface = eglCreateWindowSurface(g_render_state.egl_display, g_render_state.egl_config, (EGLNativeWindowType)out->egl_window, NULL);
    } else {
        wl_egl_window_resize(out->egl_window, width, height, 0, 0);
    }

    if (!g_gl_initialized && out->egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(g_render_state.egl_display, out->egl_surface, out->egl_surface, g_render_state.egl_context);
        render_init_gl(&g_render_state);
        mpv_init(on_mpv_update_cb);
        g_gl_initialized = true;

        if (g_wallpaper_path) {
            g_video_mode = is_video(g_wallpaper_path);
            if (g_video_mode) {
                mpv_load_file(g_wallpaper_path);
            } else {
                int w, h;
                g_texture = render_load_image(g_wallpaper_path, &w, &h);
            }
            g_wallpaper_changed = false;
        }
    }

    if (g_gl_initialized && out->egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(g_render_state.egl_display, out->egl_surface, out->egl_surface, g_render_state.egl_context);
        if (g_video_mode) {
            mpv_draw(width, height);
        } else {
            render_draw(&g_render_state, out->egl_surface, g_texture, width, height);
        }
        eglSwapBuffers(g_render_state.egl_display, out->egl_surface);
        
        // Videos need active Wayland frame callbacks to match refresh rates properly,
        // but for now, MPV's event callback loops seamlessly across multiple screens.
    }
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface) {
    (void)data;
    zwlr_layer_surface_v1_destroy(surface);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

static void setup_layer_surface_for_output(struct output *out) {
    out->wl_surface = wl_compositor_create_surface(compositor);
    out->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        layer_shell, out->wl_surface, out->wl_output, ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallpaper");

    zwlr_layer_surface_v1_set_size(out->layer_surface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(out->layer_surface, 
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(out->layer_surface, -1);

    zwlr_layer_surface_v1_add_listener(out->layer_surface, &layer_surface_listener, out);
    wl_surface_commit(out->wl_surface);
}

static void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    (void)data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version < 4 ? version : 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version < 4 ? version : 4);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct wl_output *wl_out = wl_registry_bind(registry, name, &wl_output_interface, version < 4 ? version : 4);
        struct output *out = calloc(1, sizeof(struct output));
        out->name = name; out->wl_output = wl_out; out->next = outputs; outputs = out;
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    struct output **curr = &outputs;
    while (*curr) {
        if ((*curr)->name == name) {
            struct output *to_remove = *curr; *curr = to_remove->next;
            if (to_remove->layer_surface) zwlr_layer_surface_v1_destroy(to_remove->layer_surface);
            if (to_remove->egl_surface != EGL_NO_SURFACE) eglDestroySurface(g_render_state.egl_display, to_remove->egl_surface);
            if (to_remove->egl_window) wl_egl_window_destroy(to_remove->egl_window);
            if (to_remove->wl_surface) wl_surface_destroy(to_remove->wl_surface);
            if (wl_output_get_version(to_remove->wl_output) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
                wl_output_release(to_remove->wl_output);
            } else { wl_output_destroy(to_remove->wl_output); }
            free(to_remove); break;
        }
        curr = &(*curr)->next;
    }
}

static const struct wl_registry_listener registry_listener = { .global = registry_global, .global_remove = registry_global_remove };

int main(int argc, char **argv) {
    memset(&g_config, 0, sizeof(struct app_config));
    const char *home = getenv("HOME");
    if (home) {
        snprintf(g_config_path, sizeof(g_config_path), "%s/.config/my_wallpaper_engine/config.yaml", home);
    }

    if (config_load(g_config_path, &g_config) && g_config.default_wallpaper) {
        g_wallpaper_path = strdup(g_config.default_wallpaper);
    }
    
    bool run_as_daemon = true;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-daemon") == 0) run_as_daemon = false;
        else if (argv[i][0] != '-') g_wallpaper_path = strdup(argv[i]);
    }

    if (run_as_daemon) {
        daemon(0, 0); // fork and run in background
    }

    display = wl_display_connect(NULL);
    if (!display) return EXIT_FAILURE;

    if (!render_init(display, &g_render_state)) {
        fprintf(stderr, "EGL init failed\n");
        return EXIT_FAILURE;
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !layer_shell) return EXIT_FAILURE;

    for (struct output *out = outputs; out; out = out->next) {
        setup_layer_surface_for_output(out);
    }

    int wl_fd = wl_display_get_fd(display);
    int ipc_fd = ipc_server_init();
    g_mpv_wakeup_fd = eventfd(0, EFD_NONBLOCK);
    int hypr_fd = hyprland_ipc_init();

    struct pollfd fds[4];
    int nfds = 3;
    fds[0] = (struct pollfd){ .fd = wl_fd, .events = POLLIN };
    fds[1] = (struct pollfd){ .fd = ipc_fd, .events = POLLIN };
    fds[2] = (struct pollfd){ .fd = g_mpv_wakeup_fd, .events = POLLIN };
    if (hypr_fd >= 0) {
        fds[3] = (struct pollfd){ .fd = hypr_fd, .events = POLLIN };
        nfds = 4;
    }

    printf("Wallpaper daemon started. Waiting for events...\n");
    while (1) {
        while (wl_display_prepare_read(display) != 0) {
            wl_display_dispatch_pending(display);
        }
        wl_display_flush(display);

        if (poll(fds, nfds, -1) < 0) {
            wl_display_cancel_read(display);
            break;
        }

        if (fds[0].revents & POLLIN) {
            wl_display_read_events(display);
            wl_display_dispatch_pending(display);
        } else {
            wl_display_cancel_read(display);
        }

        if (fds[1].revents & POLLIN) {
            ipc_server_handle_client(ipc_fd);
        }

        if (fds[2].revents & POLLIN) {
            uint64_t u;
            read(g_mpv_wakeup_fd, &u, sizeof(uint64_t));
            mpv_update_context();
            g_needs_redraw = true; // Tell Wayland to push new video frame
        }

        if (hypr_fd >= 0 && (fds[3].revents & POLLIN)) {
            hyprland_ipc_handle(hypr_fd, mpv_pause);
        }

        if (g_needs_redraw && g_gl_initialized && outputs) {
            g_needs_redraw = false;
            
            // Check if wallpaper pathway changed (IPC hit)
            if (g_wallpaper_changed) {
                g_wallpaper_changed = false;
                if (g_wallpaper_path) {
                    // Update persistent config on disk immediately
                    if (g_config.default_wallpaper) free(g_config.default_wallpaper);
                    g_config.default_wallpaper = strdup(g_wallpaper_path);
                    config_save(g_config_path, &g_config);

                    bool is_now_video = is_video(g_wallpaper_path);
                    
                    // If it changed, reset everything
                    if (is_now_video != g_video_mode) {
                        if (g_texture) {
                            glDeleteTextures(1, &g_texture);
                            g_texture = 0;
                        }
                    }
                    g_video_mode = is_now_video;

                    if (g_video_mode) {
                        mpv_load_file(g_wallpaper_path);
                    } else {
                        mpv_stop();
                        if (g_texture) {
                            glDeleteTextures(1, &g_texture);
                            g_texture = 0;
                        }
                        int w, h;
                        g_texture = render_load_image(g_wallpaper_path, &w, &h);
                        printf("Loaded new static wallpaper texture: %d\n", g_texture);
                    }
                }
            }
            
            // Redraw on all available outputs
            for (struct output *out = outputs; out; out = out->next) {
                if (out->egl_surface != EGL_NO_SURFACE && out->width > 0) {
                    eglMakeCurrent(g_render_state.egl_display, out->egl_surface, out->egl_surface, g_render_state.egl_context);
                    if (g_video_mode) {
                        mpv_draw(out->width, out->height);
                    } else {
                        render_draw(&g_render_state, out->egl_surface, g_texture, out->width, out->height);
                    }
                    eglSwapBuffers(g_render_state.egl_display, out->egl_surface);
                }
            }
        }
    }

    mpv_cleanup();
    render_cleanup(&g_render_state);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return EXIT_SUCCESS;
}
