#ifndef MPV_RENDER_H
#define MPV_RENDER_H

#include <stdbool.h>

// Initializes MPV and OpenGL context mapping
bool mpv_init(void (*on_update)(void));

// Loads a video/gif file into the player
void mpv_load_file(const char *path);

// Draws the current frame to the currently bound EGL surface
void mpv_draw(int width, int height);

// Updates MPV internal context safely on the main thread
void mpv_update_context(void);

// Stops active playback to prevent background rendering
void mpv_stop(void);

// Flips playback internal state for GPU optimizations
void mpv_pause(bool pause);

// Cleans up mpv handles
void mpv_cleanup(void);

#endif
