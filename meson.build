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
    'src/common/config.c',
    protos_src,
    protos_headers,
  ],
  include_directories: inc,
  dependencies: [wayland_client, wayland_egl, egl, glesv2, cairo, mpv, math, rt, yaml],
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
