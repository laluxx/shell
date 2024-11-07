#ifndef HISTORY_H
#define HISTORY_H

#include "line.h"
#include <stddef.h>

#define HISTORY_MAX 1000

typedef struct {
    char *entries[HISTORY_MAX];
    int count;
    size_t current;
} History;

void history_add(History *h, Line *line);
void handle_history(History *h, Line *line, int direction);

#endif
