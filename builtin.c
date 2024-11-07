#include "builtin.h"
#include "ansi_codes.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int is_builtin(const char *cmd) {
    return strncmp(cmd, "cd ", 3) == 0
        || strcmp(cmd, "cd") == 0
        || strcmp(cmd, "colors") == 0;
}

int handle_builtin(const char *cmd) {
    if (strncmp(cmd, "cd", 2) == 0) {
        return cd(cmd);
    }

    if (strncmp(cmd, "colors", 6) == 0) {
        return colors();
    }

    return 1; // Command not handled
}

int colors() {
    printf("COLORS\n");
    return 0;
}

// TODO PWD="~/xos" should work
int cd(const char *cmd) {
    if (strcmp(cmd, "cd") == 0) {
        const char *home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "HOME not set\n");
            return 1;
        }
        if (chdir(home) != 0) {
            perror("cd");
            return 1;
        }
        return 0;
    }

    // Skip "cd " to get the path
    const char *path = cmd + 3;

    // Trim leading spaces
    while (*path == ' ') path++;

    // Handle '~' to go to HOME directory
    if (strcmp(path, "~") == 0) {
        path = getenv("HOME");
        if (!path) {
            fprintf(stderr, "HOME not set\n");
            return 1;
        }
    }

    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }
    return 0;
}
