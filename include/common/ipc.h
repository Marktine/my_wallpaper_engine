#ifndef IPC_H
#define IPC_H

#define IPC_SOCKET_PATH "/tmp/my_wallpaper_engine.sock"

enum ipc_command_type {
    IPC_CMD_SET_WALLPAPER,
    IPC_CMD_PAUSE,
    IPC_CMD_RESUME,
    IPC_CMD_RELOAD,
    IPC_CMD_QUIT
};

struct ipc_command {
    enum ipc_command_type type;
    char payload[256];
};

#endif
