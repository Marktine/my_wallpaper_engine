#include <ncurses.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>
#include "common/ipc.h"

#define MAX_FILES 1024
#define MAX_PATH_LENGTH 1024

struct file_entry {
    char path[MAX_PATH_LENGTH];
    char name[MAX_PATH_LENGTH];
    bool is_dir;
};

struct file_entry files[MAX_FILES];
int num_files = 0;
int selected_index = 0;

// Enforces a directory "jail" so users cannot traverse above the chosen root
char g_root_dir[MAX_PATH_LENGTH] = "";

int compare_entries(const void *a, const void *b) {
    const struct file_entry *fa = (const struct file_entry *)a;
    const struct file_entry *fb = (const struct file_entry *)b;
    
    // Always pin "Go Back" to index 0
    if (strcmp(fa->name, "Go Back (..)") == 0) return -1;
    if (strcmp(fb->name, "Go Back (..)") == 0) return 1;
    
    // Sort directories before images
    if (fa->is_dir != fb->is_dir) {
        return fa->is_dir ? -1 : 1;
    }
    
    // Alphabetical sorting within groups
    return strcmp(fa->name, fb->name);
}

void scan_dir(const char *dir) {
    char current_real[MAX_PATH_LENGTH];
    if (realpath(dir, current_real) == NULL) {
        strncpy(current_real, dir, MAX_PATH_LENGTH - 1);
    }
    
    // Set root directory benchmark on first invocation to establish our boundary
    if (strlen(g_root_dir) == 0) {
        strncpy(g_root_dir, current_real, MAX_PATH_LENGTH - 1);
    }

    if (num_files >= MAX_FILES) return;
    
    DIR *d = opendir(current_real);
    if (!d) return;
    
    struct dirent *dir_ent;
    while ((dir_ent = readdir(d)) != NULL && num_files < MAX_FILES) {
        if (strcmp(dir_ent->d_name, ".") == 0) continue;

        if (strcmp(dir_ent->d_name, "..") == 0) {
            // "Jail" check: if currently at the root, completely hide the ".." shortcut
            if (strcmp(current_real, g_root_dir) == 0) {
                continue; 
            }
        }

        bool is_dir = false;
        
        if (dir_ent->d_type == DT_DIR) {
            is_dir = true;
        } else if (dir_ent->d_type == DT_REG) {
            const char *dot = strrchr(dir_ent->d_name, '.');
            if (dot && dot != dir_ent->d_name) {
                if (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0 || 
                    strcasecmp(dot, ".gif") == 0 || strcasecmp(dot, ".mp4") == 0 || strcasecmp(dot, ".mkv") == 0 || strcasecmp(dot, ".webm") == 0) {
                    is_dir = false;
                } else {
                    continue; // Skip non-media
                }
            } else {
                continue;
            }
        } else {
            continue; // Skip unknown or symlinks for now
        }

        struct file_entry *entry = &files[num_files++];
        entry->is_dir = is_dir;
        
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        if (strcmp(dir_ent->d_name, "..") == 0) {
            snprintf(entry->path, MAX_PATH_LENGTH, "%s/..", current_real);
            strcpy(entry->name, "Go Back (..)");
        } else {
            snprintf(entry->path, MAX_PATH_LENGTH, "%s/%s", current_real, dir_ent->d_name);
            strncpy(entry->name, dir_ent->d_name, MAX_PATH_LENGTH - 1);
        }
#pragma GCC diagnostic pop
    }
    closedir(d);

    // Apply the structural sorting layout
    qsort(files, num_files, sizeof(struct file_entry), compare_entries);
}

void draw_ui() {
    clear();
    attron(A_BOLD);
    mvprintw(0, 2, "My Wallpaper Engine - TUI Control Panel");
    attroff(A_BOLD);
    
    if (num_files == 0) {
        mvprintw(2, 2, "No wallpapers found! Press 'o' to open a directory.");
    } else {
        mvprintw(1, 2, "Up/Down: Navigate | Enter: Apply/Enter Folder | 'o': Open Folder | 'q': Quit");
    }
    
    for (int i = 0; i < num_files; i++) {
        if (i < selected_index - 10 || i > selected_index + 10) continue; 
        
        int display_y = 3 + (i - (selected_index > 10 ? selected_index - 10 : 0));
        
        if (i == selected_index) {
            attron(A_REVERSE);
            if (files[i].is_dir) mvprintw(display_y, 2, " > [DIR] %s ", files[i].name);
            else mvprintw(display_y, 2, " > %s ", files[i].name);
            attroff(A_REVERSE);
        } else {
            if (files[i].is_dir) mvprintw(display_y, 2, "   [DIR] %s ", files[i].name);
            else mvprintw(display_y, 2, "   %s ", files[i].name);
        }
    }
    refresh();
}

void send_ipc_wallpaper(const char *path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        struct ipc_command cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.type = IPC_CMD_SET_WALLPAPER;
        strncpy(cmd.payload, path, sizeof(cmd.payload) - 1);
        send(fd, &cmd, sizeof(cmd), 0);
    }
    close(fd);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        scan_dir(argv[1]);
    } else {
        const char *home = getenv("HOME");
        char user_path[512];
        if (home) {
            snprintf(user_path, sizeof(user_path), "%s/Wallpapers", home);
            scan_dir(user_path);
            if (num_files == 0) {
                memset(g_root_dir, 0, sizeof(g_root_dir)); // Reset root for fallback
                snprintf(user_path, sizeof(user_path), "%s/Pictures", home);
                scan_dir(user_path);
            }
        }
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    draw_ui();

    int ch;
    while ((ch = getch()) != 'q') {
        switch (ch) {
            case KEY_UP:
                if (selected_index > 0) selected_index--;
                break;
            case KEY_DOWN:
                if (selected_index < num_files - 1) selected_index++;
                break;
            case KEY_HOME:
            case 'g':
                selected_index = 0;
                break;
            case KEY_END:
            case 'G':
                if (num_files > 0) selected_index = num_files - 1;
                break;
            case 'o': {
                char input[MAX_PATH_LENGTH] = {0};
                mvprintw(LINES - 2, 2, "Enter root directory path: ");
                echo();
                curs_set(1);
                getnstr(input, MAX_PATH_LENGTH - 1);
                noecho();
                curs_set(0);
                
                if (strlen(input) > 0) {
                    num_files = 0;
                    selected_index = 0;
                    memset(g_root_dir, 0, sizeof(g_root_dir)); // Reset base jail for new directory
                    scan_dir(input);
                }
                break;
            }
            case '\n':
            case '\r':
            case KEY_ENTER:
                if (num_files > 0) {
                    if (files[selected_index].is_dir) {
                        char next_dir[MAX_PATH_LENGTH];
                        strcpy(next_dir, files[selected_index].path);
                        num_files = 0;
                        selected_index = 0;
                        scan_dir(next_dir);
                    } else {
                        send_ipc_wallpaper(files[selected_index].path);
                    }
                }
                break;
        }
        draw_ui();
    }

    endwin();
    return EXIT_SUCCESS;
}
