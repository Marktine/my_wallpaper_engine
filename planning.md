# Context
I want to build a wallpaper engine, which can be used to display my collection of wallpapers onto my screens (I have 2 screens).

# Requirements
- The wallpaper should be displayed on both screens.
- Wallpapers can be different on each screen.
- User can change their wallpapers by selecting a folder containing wallpapers or a specific file.
- The application should be able to start with the system startup.
- The application should be able to pause and resume the wallpaper display.
- The application should be able to change the wallpaper every X minutes/hours.
- The application should work on linux (arch linux) with hyprland (wayland).
- When an application is running in fullscreen, the wallpaper should be hidden or paused to save memory and performance.
- The application should have a TUI (Text User Interface) for configuration and control.

# Tech stack
- Core: C11
- Communication: `wayland-client`.
- Protocol: `wlr-layer-shell-unstable-v1`.
- Rendering: `GLES 3.0` (For GPU shaders/animations) or `Cairo` (for simple images).
- Media: `libmpv` (For video wallpapers).
- Config: `YAML` (using `libyaml`).
- TUI: `ncurses`.
