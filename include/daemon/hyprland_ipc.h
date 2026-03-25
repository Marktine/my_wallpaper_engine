#ifndef HYPRLAND_IPC_H
#define HYPRLAND_IPC_H

#include <stdbool.h>

// Initializes a UNIX connection to the Hyprland Event Socket (socket2)
int hyprland_ipc_init(void);

// Processes socket payload directly and fires dynamic pausing callbacks
void hyprland_ipc_handle(int fd, void (*on_pause)(bool));

#endif
