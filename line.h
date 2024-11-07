#ifndef LINE_H
#define LINE_H

#include <stdbool.h>

#define LINE_MAX 4096

// TODO make this a dynamic array of chars (Buffer struct)
// And keep the entire text in the terminal in a buffer 

/* typedef struct { */
/*     char buffer[LINE_MAX]; */
/*     int length; */
/*     int point; */
/*     int mark; */
/* } Line; */

typedef struct {
    char buffer[LINE_MAX];
    int length;
    int point;
    int mark;
    char* current_command;  // Current partial command
    bool is_valid_command;  // Whether current partial command matches anything
} Line;


// Syntax Highlighting
void init_command_cache(void);
void free_command_cache(void);
bool is_valid_partial_command(const char* partial);

void clear_line(Line *line);
void clear_screen(Line *line);
void set_mark(Line *line);
void kill_line(Line *line);
void kill_region(Line *line);
void delete_char(Line *line);
void backward_delete_char(Line *line);


void die(const char *error);


#endif
