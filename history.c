#include "history.h"
#include <stdlib.h>
#include <string.h>

void history_add(History *h, Line *line) {
    if (h->count >= HISTORY_MAX) {
        free(h->entries[0]);
        memmove(h->entries, h->entries + 1, (HISTORY_MAX - 1) * sizeof(char *));
        h->count--;
    }

    // Check if the current line is different from the last entry
    if (h->count == 0 || strcmp(line->buffer, h->entries[h->count - 1]) != 0) {
        h->entries[h->count] = strdup(line->buffer);
        if (!h->entries[h->count])
            die("strdup");
        h->count++;
    }

    h->current = h->count;
}

void handle_history(History *h, Line *line, int direction) {
    int new_pos = h->current + direction;

    if (new_pos < 0 || new_pos > h->count)
        return;
    if (new_pos == h->count) {
        line->length = 0;
        line->point = 0;
        line->buffer[0] = '\0';
    } else {
        strncpy(line->buffer, h->entries[new_pos], LINE_MAX - 1);
        line->length = strlen(line->buffer);
        line->point = line->length;
    }
    h->current = new_pos;
}

