#ifndef COMPLETION_H
#define COMPLETION_H

#include "line.h"
#include <sys/ioctl.h>

#define MAX_COMPLETIONS 1000
#define COMPLETION_MAX_LENGTH 256

typedef struct {
    char *items[MAX_COMPLETIONS];
    int count;
} CompletionList;

void free_completion_list(CompletionList *list);
CompletionList get_bash_completions(const char *line, int point);
int find_word_start(const char *line, int point);
void apply_completion(Line *line, const char *completion);
char *find_common_prefix(CompletionList *completions);
void handle_completion(Line *line);

#endif
