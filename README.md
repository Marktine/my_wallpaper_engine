# My Wallpaper Engine

A highly-optimized, incredibly lightweight animated wallpaper engine designed specifically for **Wayland** (and strictly optimized for **Hyprland**). Written completely from the ground up in modular **C11**, it entirely bypasses the bloat of standard modern desktop environments.

## Architecture Structure
The project is split into two asynchronous binaries that communicate entirely via local UNIX Sockets (`sys/un.h`) to keep your graphics card rendering pipeline separate from your interface interaction:

1. **`wallpaper-daemon`**: The background compositor layer.
    * Hooks directly into `wlr-layer-shell-unstable-v1` to paint underneath your active desktop windows.
    * Paints standard `.png`, `.jpg`, and `.jpeg` images instantly using pure headless `EGL / GLES3` hardware acceleration combined with `stb_image`.
    * Utilizes a highly-threaded asynchronous `libmpv` software-decoding queue to endlessly loop animated `.mp4`, `.gif`, `.mkv`, and `.webm` formats entirely off your CPU without corrupting your Mesa driver stack.
    * Silently injects an event listener into Hyprland's `socket2` IPC. The second it detects `fullscreen>>1`, it forcefully freezes the video engine, plummeting your GPU pipeline usage to **0%** so your primary active game or app doesn't suffer frame drops.
    * Can be driven completely config-first using `libyaml` on startup.

2. **`wallpaper-tui`**: The `ncurses` front-end.
    * A lightweight text-based application that dynamically searches nested directories inside `~/Wallpapers` and `/usr/share/backgrounds`.
    * Fires IPC struct payloads instantly to the background daemon, mutating its active graphics state without ever flashing or stalling Wayland.

## Requirements
* `wayland-client`, `wayland-egl`, `wayland-protocols`
* `EGL` and `GLESv2`
* `ncurses`
* `libmpv`
* `libyaml`

## Building
This project uses the `Meson` build system.
```bash
meson setup build
ninja -C build
```

## Usage
Simply start the background daemon natively, then launch the interactive TUI to browse your disk drives:
```bash
./build/wallpaper-daemon
./build/wallpaper-tui
```
*(Press `o` inside the TUI to scan custom wallpaper paths instantly!)*

## Optimizations

### Image Downscaling
To save memory and rendering resources, static wallpapers are dynamically scaled down to fit your active monitor resolution when loading. This optimization uses `stb_image_resize` to ensure high-resolution images perfectly encompass the target screen dimensions without bloating video RAM before uploading to the GPU as an OpenGL texture.
