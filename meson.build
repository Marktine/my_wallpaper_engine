project(
  'my_wallpaper_engine',
  'c',
  version: '0.1.0',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'warning_level=2',
    'werror=true',
  ],
)

cc = meson.get_compiler('c')

# Dependencies
wayland_client = dependency('wayland-client')
wayland_scanner = dependency('wayland-scanner', version: '>=1.14.91')
wayland_protos = dependency('wayland-protocols', version: '>=1.24')
wayland_egl = dependency('wayland-egl')

xkbcommon = cc.find_library('xkbcommon', required: false)
cairo = dependency('cairo', required: false)
egl = dependency('egl', required: false)
glesv2 = dependency('glesv2', required: false)
mpv = dependency('mpv', required: true)
yaml = dependency('yaml-0.1', required: false)
ncurses = dependency('ncursesw', required: false)
math = cc.find_library('m', required: false)
rt = cc.find_library('rt', required: false)
dl = cc.find_library('dl', required: false)
threads = dependency('threads')

# gtk4-layer-shell MUST be declared before wayland_client so the dynamic
# linker loads libgtk4-layer-shell.so before libwayland-client.so. The
# library shims wl_proxy_create() calls at load time; if libwayland-client
# wins the race it's too late and compositor layer surfaces won't work.
gtk4 = dependency('gtk4', required: false)
gtk4_layer_shell = dependency('gtk4-layer-shell-0', required: false)

# Wayland Scanner Protocol Generation
wayland_scanner_prog = find_program(wayland_scanner.get_variable(pkgconfig: 'wayland_scanner'))
wl_protocol_dir = wayland_protos.get_variable(pkgconfig: 'pkgdatadir')

scanner_code = generator(
  wayland_scanner_prog,
  output: '@BASENAME@-protocol.c',
  arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

scanner_client_header = generator(
  wayland_scanner_prog,
  output: '@BASENAME@-client-protocol.h',
  arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

# Core Protocols
protocol_files = [
  'protocols/wlr-layer-shell-unstable-v1.xml',
  join_paths(wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'),
]

protos_src = []
protos_headers = []

foreach xml : protocol_files
  protos_src += scanner_code.process(xml)
  protos_headers += scanner_client_header.process(xml)
endforeach

inc = include_directories('include')

# Daemon Component
executable(
  'wallpaper-daemon',
  sources: [
    'src/daemon/main.c',
    'src/daemon/render.c',
    'src/daemon/stb_image_impl.c',
    'src/daemon/server_ipc.c',
    'src/daemon/mpv_render.c',
    'src/daemon/hyprland_ipc.c',
    'src/daemon/plugin_manager.c',
    'src/common/config.c',
    protos_src,
    protos_headers,
  ],
  include_directories: inc,
  # gtk4_layer_shell MUST be first here: it shims libwayland-client symbols and
  # must appear before wayland_client in the link order.
  dependencies: [gtk4_layer_shell, gtk4, wayland_client, wayland_egl, egl, glesv2, cairo, mpv, math, rt, yaml, dl],
  install: true,
)

# TUI Component
executable(
  'wallpaper-tui',
  sources: [
    'src/tui/main.c',
  ],
  include_directories: inc,
  dependencies: [ncurses, yaml],
  install: true,
)

# Plugins
# gtk4 and gtk4_layer_shell already declared above (before wayland_client)

if gtk4.found() and gtk4_layer_shell.found()
  shared_module(
    'sample_gtk_widget',
    'src/plugins/sample_gtk_widget.c',
    include_directories: inc,
    dependencies: [gtk4, gtk4_layer_shell, threads],
    name_prefix: '',
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'my_wallpaper_engine', 'plugins')
  )

  shared_module(
    'analog_clock',
    'src/plugins/analog_clock.c',
    include_directories: inc,
    dependencies: [gtk4, gtk4_layer_shell, threads, math],
    name_prefix: '',
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'my_wallpaper_engine', 'plugins')
  )

  shared_module(
    'calendar',
    'src/plugins/calendar.c',
    include_directories: inc,
    dependencies: [gtk4, gtk4_layer_shell, threads, math],
    name_prefix: '',
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'my_wallpaper_engine', 'plugins')
  )
endif
