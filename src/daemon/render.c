#include "daemon/render.h"
#include "daemon/stb_image.h"
#include "daemon/stb_image_resize.h"
#include <stdio.h>
#include <stdlib.h>
#include <wayland-egl.h>

bool render_init(struct wl_display *wl_display, struct render_state *state) {
    state->egl_display = eglGetDisplay((EGLNativeDisplayType)wl_display);
    if (state->egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display\n");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(state->egl_display, &major, &minor)) return false;

    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };

    EGLint num_config;
    if (!eglChooseConfig(state->egl_display, config_attribs, &state->egl_config, 1, &num_config) || num_config == 0) return false;

    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    state->egl_context = eglCreateContext(state->egl_display, state->egl_config, EGL_NO_CONTEXT, context_attribs);
    if (state->egl_context == EGL_NO_CONTEXT) return false;

    return true;
}

static const char *vs_src =
    "#version 300 es\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "out vec2 TexCoord;\n"
    "void main() { gl_Position = vec4(aPos, 0.0, 1.0); TexCoord = aTexCoord; }\n";

static const char *fs_src =
    "#version 300 es\n"
    "precision mediump float;\n"
    "out vec4 FragColor;\n"
    "in vec2 TexCoord;\n"
    "uniform sampler2D tex1;\n"
    "void main() { FragColor = texture(tex1, TexCoord); }\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

bool render_init_gl(struct render_state *state) {
    // EGL context must be current using a dummy pbuffer, but since we don't have one, 
    // the caller MUST make sure the context is current with a surface BEFORE calling this!
    // Wait, eglMakeCurrent requires a surface. Let's assume the caller handles this or we create a pbuffer.
    // We will do it in main.c during the first surface's configure.
    
    GLuint vso = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fso = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    state->shader_program = glCreateProgram();
    glAttachShader(state->shader_program, vso);
    glAttachShader(state->shader_program, fso);
    glLinkProgram(state->shader_program);
    glDeleteShader(vso);
    glDeleteShader(fso);

    float vertices[] = {
        // positions   // texCoords
         1.0f,  1.0f,  1.0f, 1.0f, // top right
         1.0f, -1.0f,  1.0f, 0.0f, // bottom right
        -1.0f, -1.0f,  0.0f, 0.0f, // bottom left
        -1.0f,  1.0f,  0.0f, 1.0f  // top left 
    };
    unsigned int indices[] = { 0, 1, 3, 1, 2, 3 };

    glGenVertexArrays(1, &state->vao);
    glGenBuffers(1, &state->vbo);
    GLuint ebo;
    glGenBuffers(1, &ebo);

    glBindVertexArray(state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    return true;
}

GLuint render_load_image(const char *path, int *width, int *height, int target_w, int target_h) {
    // In OpenGL context!
    int channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path, width, height, &channels, 4);
    if (!data) {
        fprintf(stderr, "Failed to load image: %s\n", path);
        return 0;
    }

    if (target_w > 0 && target_h > 0 && (*width > target_w || *height > target_h)) {
        // Keep aspect ratio covering the target dimensions
        float scale_w = (float)target_w / *width;
        float scale_h = (float)target_h / *height;
        float scale = (scale_w > scale_h) ? scale_w : scale_h;
        if (scale > 1.0f) scale = 1.0f; // Only scale down
        
        int new_w = *width * scale;
        int new_h = *height * scale;
        
        unsigned char *resized_data = malloc(new_w * new_h * 4);
        if (resized_data) {
            stbir_resize_uint8(data, *width, *height, 0, resized_data, new_w, new_h, 0, 4);
            stbi_image_free(data);
            data = resized_data;
            *width = new_w;
            *height = new_h;
        }
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *width, *height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);
    return texture;
}

void render_draw(struct render_state *state, EGLSurface egl_surface, GLuint texture, int win_w, int win_h) {
    eglMakeCurrent(state->egl_display, egl_surface, egl_surface, state->egl_context);
    
    // MPV aggressively clobbers OpenGL state during video playback.
    // We must reset these explicit rasterization states before rendering the static image.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glViewport(0, 0, win_w, win_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(state->shader_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(state->shader_program, "tex1"), 0);

    glBindVertexArray(state->vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    
    // eglSwapBuffers is intentionally handled exclusively by the orchestrating main loop
}

void render_cleanup(struct render_state *state) {
    if (state->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(state->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (state->egl_context != EGL_NO_CONTEXT) eglDestroyContext(state->egl_display, state->egl_context);
        eglTerminate(state->egl_display);
    }
}
