#define _POSIX_C_SOURCE 200809L
#include "daemon/server_ipc.h"
#include "common/ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>

extern const char *g_wallpaper_path;

int ipc_server_init(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(IPC_SOCKET_PATH);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 5) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    printf("IPC server listening on %s\n", IPC_SOCKET_PATH);
    return fd;
}

void ipc_server_handle_client(int server_fd) {
    struct sockaddr_un client_addr;
    socklen_t len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
    if (client_fd < 0) {
        perror("accept");
        return;
    }

    struct ipc_command cmd;
    ssize_t bytes = recv(client_fd, &cmd, sizeof(cmd), 0);
    if (bytes == sizeof(cmd)) {
        printf("Received IPC command: type=%d, payload='%s'\n", cmd.type, cmd.payload);
        
        switch (cmd.type) {
            case IPC_CMD_SET_WALLPAPER:
                if (g_wallpaper_path) free((void *)g_wallpaper_path);
                g_wallpaper_path = strdup(cmd.payload);
                extern bool g_needs_redraw;
                extern bool g_wallpaper_changed;
                g_needs_redraw = true;
                g_wallpaper_changed = true;
                printf("Wallpaper path set to: %s\n", g_wallpaper_path);
                break;
            case IPC_CMD_QUIT:
                // Handle quit
                printf("Quit command received. Exiting...\n");
                exit(EXIT_SUCCESS);
                break;
            default:
                break;
        }
    }
    
    close(client_fd);
}
