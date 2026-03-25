#ifndef SERVER_IPC_H
#define SERVER_IPC_H

// Initializes the UNIX domain socket server and returns the file descriptor
int ipc_server_init(void);

// Accepts a client connection and reads a command
void ipc_server_handle_client(int server_fd);

#endif
