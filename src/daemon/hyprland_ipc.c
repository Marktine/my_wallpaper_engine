#include "daemon/hyprland_ipc.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int hyprland_ipc_init(void) {
    const char *his = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    const char *xrd = getenv("XDG_RUNTIME_DIR");
    if (!his || !xrd) {
        printf("Hyprland environment variables not detected. GPU optimization bypassed.\n");
        return -1;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/hypr/%s/.socket2.sock", xrd, his);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Failed to connect to Hyprland event socket.\n");
        close(fd);
        return -1;
    }
    
    printf("Successfully hooked into Hyprland compositor for optimization tracking!\n");
    return fd;
}

void hyprland_ipc_handle(int fd, void (*on_pause)(bool)) {
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;
    buf[n] = '\0';
    
    // Detect global fullscreen toggles across any monitor spaces
    if (strstr(buf, "fullscreen>>1")) {
        on_pause(true); // Freeze rendering
    } else if (strstr(buf, "fullscreen>>0")) {
        on_pause(false); // Thaw rendering
    }
}
