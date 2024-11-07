#include "clipboard.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

char *get_clipboard_from_system() {
    int pipefd[2];
    pid_t pid;
    char *content = NULL;
    size_t content_size = 0;
    char buffer[1024];
    
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return NULL;
    }
    
    pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }
    
    if (pid == 0) {  // Child process
        close(pipefd[0]);  // Close read end
        
        // Redirect stdout to pipe
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        
        // Execute xclip -o to output clipboard content
        execlp("xclip", "xclip", "-selection", "clipboard", "-o", NULL);
        perror("execlp");
        exit(1);
    }
    
    // Parent process
    close(pipefd[1]);  // Close write end
    
    ssize_t bytes_read;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        char *new_content = realloc(content, content_size + bytes_read + 1);
        if (!new_content) {
            free(content);
            close(pipefd[0]);
            return NULL;
        }
        content = new_content;
        memcpy(content + content_size, buffer, bytes_read);
        content_size += bytes_read;
    }
    
    close(pipefd[0]);
    
    if (content) {
        content[content_size] = '\0';
    }
    
    // Wait for child process
    int status;
    waitpid(pid, &status, 0);
    
    return content;
}

bool set_system_clipboard(const char *clipboard) {
    int pipefd[2];
    pid_t pid;
    
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return false;
    }
    
    pid = fork();
    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }
    
    if (pid == 0) {  // Child process
        close(pipefd[1]);  // Close write end
        
        // Redirect stdin to pipe
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        
        // Execute xclip to set clipboard content
        execlp("xclip", "xclip", "-selection", "clipboard", NULL);
        perror("execlp");
        exit(1);
    }
    
    // Parent process
    close(pipefd[0]);  // Close read end
    
    // Write clipboard to pipe
    size_t len = strlen(clipboard);
    ssize_t written = write(pipefd[1], clipboard, len);
    
    close(pipefd[1]);
    
    // Wait for child process
    int status;
    waitpid(pid, &status, 0);
    
    return written == len;
}

// TODO FIX yanking more than one line
void yank(Line *line, char *clipboard) {
    free(clipboard);
    
    clipboard = get_clipboard_from_system();
    if (!clipboard) {
        return;
    }
    
    // Calculate space needed
    size_t paste_len = strlen(clipboard);
    if (line->length + paste_len >= LINE_MAX - 1) {
        paste_len = LINE_MAX - 1 - line->length;
    }
    
    if (paste_len == 0) {
        return;
    }
    
    // Make space for new content
    memmove(line->buffer + line->point + paste_len,
            line->buffer + line->point,
            line->length - line->point);
    
    // Insert clipboard content
    memcpy(line->buffer + line->point, clipboard, paste_len);
    line->point += paste_len;
    line->length += paste_len;
    line->buffer[line->length] = '\0';
    
    clear_line(line);
}

