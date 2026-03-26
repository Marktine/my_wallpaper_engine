#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

struct render_state {
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLConfig egl_config;
    
    // GL objects
    GLuint shader_program;
    GLuint vao;
    GLuint vbo;
};

// Initializes EGL display and context
bool render_init(struct wl_display *wl_display, struct render_state *state);

// Initializes OpenGL primitives (Shaders, VBOs, VAOs)
bool render_init_gl(struct render_state *state);

// Loads an image from disk into a GL texture
GLuint render_load_image(const char *path, int *width, int *height, int target_w, int target_h);

// Binds the surface, draws the texture, and swaps buffers
void render_draw(struct render_state *state, EGLSurface egl_surface, GLuint texture, int win_w, int win_h);

// Cleans up EGL resources
void render_cleanup(struct render_state *state);

#endif
