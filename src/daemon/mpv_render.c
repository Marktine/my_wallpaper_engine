#include "daemon/mpv_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <mpv/client.h>
#include <mpv/render_gl.h>

static mpv_handle *mpv_ctx = NULL;
static mpv_render_context *mpv_gl = NULL;
static void (*update_cb)(void) = NULL;

static void on_mpv_render_update(void *ctx) {
    (void)ctx;
    if (update_cb) update_cb();
}

static void *get_proc_address(void *ctx, const char *name) {
    (void)ctx;
    return (void *)eglGetProcAddress(name);
}

bool mpv_init(void (*on_update)(void)) {
    mpv_ctx = mpv_create();
    if (!mpv_ctx) return false;

    // hwdec=auto-copy: Uses the GPU's fixed-function video decoder (VAAPI/VDPAU)
    // to offload heavy H.264/H.265 decoding from shader cores, but copies the
    // decoded frame through CPU memory before GL upload. This avoids the Mesa
    // EGL interop segfaults that hwdec=auto causes in layer-shell contexts,
    // while still being far lighter on GPU than full software decoding.
    mpv_set_option_string(mpv_ctx, "hwdec", "auto-copy");
    
    mpv_set_option_string(mpv_ctx, "loop", "inf");   // Loop wallpapers
    mpv_set_option_string(mpv_ctx, "vo", "libmpv");  // Engine direct render
    mpv_set_option_string(mpv_ctx, "mute", "yes");   // Mute volume completely
    mpv_set_option_string(mpv_ctx, "ao", "null");    // Do not initialize audio backend

    if (mpv_initialize(mpv_ctx) < 0) return false;

    update_cb = on_update;

    mpv_opengl_init_params gl_init_params = {
        .get_proc_address = get_proc_address,
    };
    
    // Disable advanced controls as we don't manage complex FBO swaps
    int advanced = 0;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advanced},
        {MPV_RENDER_PARAM_INVALID, NULL}
    };

    if (mpv_render_context_create(&mpv_gl, mpv_ctx, params) < 0) {
        mpv_terminate_destroy(mpv_ctx);
        mpv_ctx = NULL;
        return false;
    }

    mpv_render_context_set_update_callback(mpv_gl, on_mpv_render_update, NULL);
    return true;
}

void mpv_load_file(const char *path) {
    if (!mpv_ctx) return;
    const char *cmd[] = {"loadfile", path, NULL};
    mpv_command_async(mpv_ctx, 0, cmd);
}

void mpv_stop(void) {
    if (!mpv_ctx) return;
    const char *cmd[] = {"stop", NULL};
    mpv_command_async(mpv_ctx, 0, cmd);
}

void mpv_pause(bool pause) {
    if (!mpv_ctx) return;
    const char *val = pause ? "yes" : "no";
    mpv_set_property_string(mpv_ctx, "pause", val);
}

void mpv_update_context(void) {
    if (mpv_gl) {
        mpv_render_context_update(mpv_gl);
    }
}

void mpv_draw(int width, int height) {
    if (!mpv_gl) return;
    
    mpv_opengl_fbo fbo = {
        .fbo = 0,
        .w = width,
        .h = height,
        .internal_format = 0
    };

    int flip_y = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, NULL}
    };

    mpv_render_context_render(mpv_gl, params);
}

#include <malloc.h>

void mpv_cleanup(void) {
    if (mpv_gl) {
        mpv_render_context_free(mpv_gl);
        mpv_gl = NULL;
    }
    if (mpv_ctx) {
        mpv_terminate_destroy(mpv_ctx);
        mpv_ctx = NULL;
    }
    // Force glibc to return freed memory chunks to the Linux kernel.
    // Without this, the process RSS often remains massive after a video closes.
    malloc_trim(0);
}
