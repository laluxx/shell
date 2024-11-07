#include "prompt.h"
#include "ansi_codes.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int status = 0;

void draw_prompt(int status) {
    char hostname[MAX_HOSTNAME];
    char username[MAX_USERNAME];
    char path[MAX_PATH];
    
    // Get username
    if (getlogin_r(username, sizeof(username)) != 0) {
        strncpy(username, "unknown", sizeof(username));
    }
    
    // Get hostname
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname));
    }
    
    // Get current directory
    get_current_dir(path, sizeof(path));
    
    // Print prompt with colors
    printf("\r" CLEAR_LINE);  // Clear the line first
    printf(GREEN "%s@%s" COLOR_RESET " ", username, hostname);
    printf(BLUE "%s" COLOR_RESET " ", path);
    printf("%s" "λ" COLOR_RESET " ", status == 0 ? GREEN : RED);
    fflush(stdout);
}

void get_current_dir(char *path, size_t size) {
    char *home = getenv("HOME");
    char cwd[MAX_PATH];
    
    if (!getcwd(cwd, sizeof(cwd))) {
        strncpy(path, ".", size);
        return;
    }
    
    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        path[0] = '~';
        strncpy(path + 1, cwd + strlen(home), size - 1);
    } else {
        strncpy(path, cwd, size);
    }
}

