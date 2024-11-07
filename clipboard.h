#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include "line.h"
#include <stdbool.h>

extern char *clipboard;

char *get_clipboard_from_system();
bool set_system_clipboard(const char *clipboard);
void yank(Line *line, char *clipboard);

#endif
