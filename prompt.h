#ifndef PROMPT_H
#define PROMPT_H

#include <stddef.h>

#define MAX_HOSTNAME 256
#define MAX_PATH 4096
#define MAX_USERNAME 256

extern int status;

void draw_prompt(int status);
void get_current_dir(char *path, size_t size);

#endif
