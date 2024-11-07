#include "line.h"
#include "prompt.h"
#include "ansi_codes.h"
#include "electric_pair_mode.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

// TODO Make a struct
static char** cached_commands = NULL;
static size_t cached_commands_count = 0;
static size_t cached_commands_capacity = 0;

// Initialize command cache on startup
void init_command_cache(void) {
    cached_commands_capacity = 1024;  // Initial capacity
    cached_commands = malloc(cached_commands_capacity * sizeof(char*));
    cached_commands_count = 0;
    
    char* path = getenv("PATH");
    if (!path) return;
    
    char* path_copy = strdup(path);
    char* dir_str = strtok(path_copy, ":");
    
    while (dir_str) {
        DIR* dir = opendir(dir_str);
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                char full_path[LINE_MAX];
                snprintf(full_path, sizeof(full_path), "%s/%s", dir_str, entry->d_name);
                
                // Check if file is executable
                if (access(full_path, X_OK) == 0) {
                    // Resize if needed
                    if (cached_commands_count >= cached_commands_capacity) {
                        cached_commands_capacity *= 2;
                        cached_commands = realloc(cached_commands, 
                                                  cached_commands_capacity * sizeof(char*));
                    }
                    cached_commands[cached_commands_count++] = strdup(entry->d_name);
                }
            }
            closedir(dir);
        }
        dir_str = strtok(NULL, ":");
    }
    
    free(path_copy);
}

void free_command_cache(void) {
    for (size_t i = 0; i < cached_commands_count; i++) {
        free(cached_commands[i]);
    }
    free(cached_commands);
    cached_commands = NULL;
    cached_commands_count = 0;
}


// TODO Why cd is not found but c does ? 
bool is_valid_partial_command(const char* partial) {
    if (!partial || !*partial) return false;
    
    size_t partial_len = strlen(partial);
    for (size_t i = 0; i < cached_commands_count; i++) {
        // Only return true if we find an EXACT match
        if (strlen(cached_commands[i]) == partial_len && 
            strcmp(cached_commands[i], partial) == 0) {
            return true;
        }
    }
    return false;
}


void clear_line(Line *line) {
    printf("\r");
    draw_prompt(status);
    
    if (line->length > 0) {
        // Find first word (command)
        char* space = strchr(line->buffer, ' ');
        int cmd_len = space ? (space - line->buffer) : line->length;
        
        // Only check command if it changed
        if (!line->current_command || 
            strncmp(line->current_command, line->buffer, cmd_len) != 0 || 
            (line->current_command[cmd_len] != '\0')) {
            
            // Update current command
            free(line->current_command);
            line->current_command = strndup(line->buffer, cmd_len);
            line->is_valid_command = is_valid_partial_command(line->current_command);
        }
        
        // Print command portion with color
        printf("%s%.*s%s", 
               line->is_valid_command ? GREEN : RED,
               cmd_len, 
               line->buffer,
               COLOR_RESET);
        
        // Print rest of line if any
        if (cmd_len < line->length) {
            printf("%s", line->buffer + cmd_len);
        }
    }
    
    // Move cursor to correct position
    if (line->point < line->length) {
        for (int i = 0; i < (line->length - line->point); i++) {
            printf(CURSOR_LEFT);
        }
    }
    
    fflush(stdout);
}

/* void clear_line(Line *line) { */
/*     draw_prompt(status); */
/*     printf("%s", line->buffer); */
/*     int pos = line->length - line->point; */
/*     while (pos-- > 0) */
/*         printf(CURSOR_LEFT); */
/*     fflush(stdout); */
/* } */

void clear_screen(Line *line) {
    printf(CLEAR_SCREEN);
    clear_line(line);
    fflush(stdout);
}

// TODO Render region
void set_mark(Line *line) {
    line->mark = line->point;
}

void kill_line(Line *line) {
    line->length = line->point;
    line->buffer[line->length] = '\0';
    clear_line(line);
}

void kill_region(Line *line) {
    if (line->point == line->mark) return;

    int start = (line->point < line->mark) ? line->point : line->mark;
    int end = (line->point > line->mark) ? line->point : line->mark;

    memmove(line->buffer + start, line->buffer + end, line->length - end);
    line->length -= (end - start);
    line->buffer[line->length] = '\0';
    line->point = start;
    line->mark = 0;
    clear_line(line);
}

void delete_char(Line *line) {
    if (line->point < line->length) {
        memmove(line->buffer + line->point,
                line->buffer + line->point + 1,
                line->length - line->point);

        line->length--;
        line->buffer[line->length] = '\0';
        clear_line(line);
    }
}

void backward_delete_char(Line *line) {
    if (line->point > 0) {
        // Check if we're between a matching pair of delimiters
        if (line->point < line->length
            && is_opening_delimiter(line->buffer[line->point - 1])
            && is_matching_pair(line->buffer[line->point - 1], line->buffer[line->point])
            && electric_pair_mode) {
            
            // Delete both characters
            memmove(line->buffer + line->point - 1, 
                    line->buffer + line->point + 1,
                    line->length - line->point - 1);
            line->point--;
            line->length -= 2;
            line->buffer[line->length] = '\0';
        } else {
            // Normal single character deletion
            memmove(line->buffer + line->point - 1, 
                    line->buffer + line->point,
                    line->length - line->point);
            line->point--;
            line->length--;
            line->buffer[line->length] = '\0';
        }
    }
}


// NOTE This function is defined here
// only because the line module is common
// and we need this function everywhere
void die(const char *error) {
    perror(error);
    exit(1);
}
