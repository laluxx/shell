#include "completion.h"
#include "prompt.h"
#include "ansi_codes.h"
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void free_completion_list(CompletionList *list) {
    for (int i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    list->count = 0;
}

CompletionList get_bash_completions(const char *line, int point) {
    CompletionList completions = {0};
    char command[1024];
    
    // Create a temporary file to store completions
    char temp_file[] = "/tmp/completion_XXXXXX";
    int fd = mkstemp(temp_file);
    if (fd == -1) return completions;
    close(fd);

    // Prepare the completion command
    // This complex command:
    // 1. Sources bash completion scripts
    // 2. Sets up completion environment
    // 3. Gets completions for current command line
    snprintf(command, sizeof(command),
             "bash -c '\n"
             "source /usr/share/bash-completion/bash_completion 2>/dev/null\n"
             "COMP_LINE=\"%s\"\n"
             "COMP_POINT=%d\n"
             "COMP_TYPE=9\n" // TAB completion type
             "_completion_loader \"${COMP_LINE%% *}\" 2>/dev/null\n"
             "$(complete -p \"${COMP_LINE%% *}\" 2>/dev/null | sed \"s/.*-F \\([^ ]*\\).*/\\1/\") 2>/dev/null\n"
             "if [ -n \"${COMPREPLY[*]}\" ]; then\n"
             "  printf \"%%s\\n\" \"${COMPREPLY[@]}\"\n"
             "else\n"
             "  compgen -o default -- \"${COMP_LINE##* }\" 2>/dev/null\n"
             "fi' > %s", 
             line, point, temp_file);

    // Execute the completion command
    system(command);

    // Read completions from temporary file
    FILE *fp = fopen(temp_file, "r");
    if (fp) {
        char buffer[COMPLETION_MAX_LENGTH];
        while (fgets(buffer, sizeof(buffer), fp) && completions.count < MAX_COMPLETIONS) {
            // Remove newline
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len-1] == '\n') {
                buffer[len-1] = '\0';
            }
            
            completions.items[completions.count] = strdup(buffer);
            completions.count++;
        }
        fclose(fp);
    }

    unlink(temp_file);
    return completions;
}

int find_word_start(const char *line, int point) {
    int i = point;
    while (i > 0 && !isspace(line[i - 1])) {
        i--;
    }
    return i;
}

void apply_completion(Line *line, const char *completion) {
    int word_start = find_word_start(line->buffer, line->point);
    
    // Calculate the part of the word we're completing
    int word_len = line->point - word_start;
    int completion_len = strlen(completion);
    
    // Make sure we have enough space
    if (line->length - word_len + completion_len >= LINE_MAX - 1) {
        return;
    }
    
    // Remove the current partial word
    memmove(line->buffer + word_start, 
            line->buffer + line->point,
            line->length - line->point);
    line->length -= word_len;
    
    // Insert the completion
    memmove(line->buffer + word_start + completion_len,
            line->buffer + word_start,
            line->length - word_start);
    memcpy(line->buffer + word_start, completion, completion_len);
    
    line->length += completion_len;
    line->point = word_start + completion_len;
    line->buffer[line->length] = '\0';
}

char *find_common_prefix(CompletionList *completions) {
    if (completions->count == 0) return NULL;
    
    char *prefix = strdup(completions->items[0]);
    int prefix_len = strlen(prefix);
    
    for (int i = 1; i < completions->count; i++) {
        int j;
        for (j = 0; j < prefix_len; j++) {
            if (prefix[j] != completions->items[i][j]) {
                prefix[j] = '\0';
                prefix_len = j;
                break;
            }
        }
    }
    
    return prefix;
}

void handle_completion(Line *line) {
    CompletionList completions = get_bash_completions(line->buffer, line->point);
    
    if (completions.count == 0) {
        // No completions available
        return;
    } else if (completions.count == 1) {
        // Single completion - apply it directly
        apply_completion(line, completions.items[0]);
    } else {
        // Multiple completions
        // First try to apply common prefix
        char *common_prefix = find_common_prefix(&completions);
        if (common_prefix && strlen(common_prefix) > 0) {
            apply_completion(line, common_prefix);
            free(common_prefix);
        }
        
        // Display all possible completions
        printf("\n");
        int max_length = 0;
        for (int i = 0; i < completions.count; i++) {
            int len = strlen(completions.items[i]);
            if (len > max_length) max_length = len;
        }
        
        // Calculate number of columns based on terminal width
        struct winsize ws;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
        int cols = ws.ws_col / (max_length + 2);
        if (cols == 0) cols = 1;
        
        // Print completions in columns
        for (int i = 0; i < completions.count; i++) {
            printf("%-*s%s", max_length + 2, completions.items[i],
                   (i + 1) % cols == 0 ? "\n" : "");
        }
        if (completions.count % cols != 0) printf("\n");
    }
    
    // Redraw prompt and line
    draw_prompt(status);
    printf("%s", line->buffer);
    int pos = line->length - line->point;
    while (pos-- > 0) printf(CURSOR_LEFT);
    
    free_completion_list(&completions);
}
