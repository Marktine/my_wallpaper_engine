# My Wallpaper Engine

An incredibly lightweight animated wallpaper engine designed specifically for **Wayland** (and strictly optimized for **Hyprland**). Written completely from the ground up in modular **C11**, it entirely bypasses the bloat of standard modern desktop environments.

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

## Plugin System (GTK4)
The engine now supports a sophisticated, modular plugin system that allows you to overlay interactive **GTK4** widgets directly onto your background layer. 

### 📅 Retro Calendar Plugin
A stunning, pixel-art styled calendar widget designed to be non-intrusive yet highly informative.
- **Smart Hover Scaling**: The widget naturally shrinks into a tiny "disk" showing only the current date when your mouse is away. It instantly expands into a full month grid upon hover.
- **Live Google Calendar Sync**: Automatically pulls your real meetings, events, and schedules directly from the Google Calendar API.
- **International Holiday Support**: Pre-configured to display public holidays for **Vietnam (🇻🇳)** and **Japan (🇯🇵)**, complete with flag icons.
- **Interactive Tooltips**: Hover over any day in the grid to see a stylish, Pango-markup styled detail box listing your schedule and holidays.
- **Direct Re-Auth**: Features a built-in `🔑` button to quickly refresh your Google OAuth session without ever opening a terminal.

### Setting up Google Calendar
To enable the calendar sync and holiday features:

1.  **GCP Setup**: Create a project in the Google Cloud Console, enable the Calendar API, and download your `credentials.json`.
2.  **Configuration**: Place the file at `~/.config/my_wallpaper_engine/credentials.json`.
3.  **Authentication**: Run the fetcher script at least once to authorize via your browser.
4.  **Automation**: Add a cron job to keep your calendar updated automatically.

## Requirements
* `wayland-client`, `wayland-egl`, `wayland-protocols`
* `EGL` and `GLESv2`
* `gtk4` and `gtk4-layer-shell` (needed for plugins)
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
