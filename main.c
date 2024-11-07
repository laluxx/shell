#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>

#define HISTORY_MAX 1000
#define LINE_MAX 4096
#define MAX_HOSTNAME 256
#define MAX_PATH 4096
#define MAX_USERNAME 256

#define CLEAR_SCREEN "\x1b[2J\x1b[H"
#define CLEAR_LINE "\x1b[2K"
#define CURSOR_LEFT "\x1b[D"
#define CURSOR_RIGHT "\x1b[C"

#define GREEN "\x1b[32m"
#define RED "\x1b[31m"
#define BLUE "\x1b[34m"
#define COLOR_RESET "\x1b[0m"

int status = 0;
bool electric_pair_mode = true;
char *clipboard = NULL;

typedef struct {
    char *entries[HISTORY_MAX];
    int count;
    size_t current;
} History;

// TODO make this a dynamic array of chars (Buffer struct)
// And keep the entire text in the terminal in a buffer 
typedef struct {
    char buffer[LINE_MAX];
    int length;
    int point;
    int mark;
} Line;

History history = {0};
Line line = {0};
struct termios orig_termios;

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


void die(const char *error) {
    perror(error);
    exit(1);
}

void disable_raw_mode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        die("tcgetattr");
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        die("tcsetattr");
}

void history_add(Line *line) {
    if (history.count >= HISTORY_MAX) {
        free(history.entries[0]);
        memmove(history.entries, history.entries + 1, (HISTORY_MAX - 1) * sizeof(char *));
        history.count--;
    }

    // Check if the current line is different from the last entry
    if (history.count == 0 || strcmp(line->buffer, history.entries[history.count - 1]) != 0) {
        history.entries[history.count] = strdup(line->buffer);
        if (!history.entries[history.count])
            die("strdup");
        history.count++;
    }

    history.current = history.count;
}

void clear_line(Line *line) {
    draw_prompt(status);
    printf("%s", line->buffer);
    int pos = line->length - line->point;
    while (pos-- > 0)
        printf(CURSOR_LEFT);
    fflush(stdout);
}

void clear_screen(Line *line) {
    printf(CLEAR_SCREEN);
    clear_line(line);
    fflush(stdout);
}


bool is_closing_delimiter(char c) {
    return c == ')' || c == ']' || c == '}' || c == '"' || c == '\'';
}

bool is_opening_delimiter(char c) {
    return c == '(' || c == '[' || c == '{' || c == '"' || c == '\'';
}

char get_matching_char(char c) {
    switch (c) {
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
    case '"': return '"';
    case '\'': return '\'';
    default: return '\0';
    }
}

// TODO fix ( ( | ) ) when you thipe ')' this should happen ( (  )| ) not this ( (  ) )|
int find_next_closing_delimiter(const char *buffer, int point, char c) {
    char opening;
    switch (c) {
    case ')': opening = '('; break;
    case ']': opening = '['; break;
    case '}': opening = '{'; break;
    case '"': 
    case '\'': opening = c; break;
    default: return -1;
    }
    
    // For quotes, find the next quote
    if (c == '"' || c == '\'') {
        int count = 0;
        // Count quotes before our position
        for (int i = 0; i < point; i++) {
            if (buffer[i] == c) count++;
        }
        // If we have an odd number before us, find the closing quote
        if (count % 2 == 1) {
            for (int i = point; buffer[i] != '\0'; i++) {
                if (buffer[i] == c) return i;
            }
        }
        return -1;
    }
    
    // For brackets, find the nearest unclosed matching bracket
    int depth = 0;
    // Count depth before our position
    for (int i = 0; i < point; i++) {
        if (buffer[i] == opening) depth++;
        else if (buffer[i] == c) depth--;
    }
    
    // If we're inside brackets, find the next matching close
    if (depth > 0) {
        for (int i = point; buffer[i] != '\0'; i++) {
            if (buffer[i] == opening) depth++;
            else if (buffer[i] == c) {
                depth--;
                if (depth == 0) return i;
            }
        }
    }
    return -1;
}

// Check if we're inside an open pair of delimiters
bool is_inside_delimiters(const char *buffer, int point) {
    for (int i = 0; i < point; i++) {
        if (is_opening_delimiter(buffer[i])) {
            char matching = get_matching_char(buffer[i]);
            int next_closer = find_next_closing_delimiter(buffer, i + 1, matching);
            if (next_closer > point) return true;
        }
    }
    return false;
}

void insert_char(char c) {
    if (line.length >= LINE_MAX - 2)
        return;

    // If it's a closing delimiter, check if we should jump to after the closing one
    if (is_closing_delimiter(c)) {
        // If we're exactly on a closing delimiter
        if (line.point < line.length && line.buffer[line.point] == c) {
            line.point++;
            clear_line(&line);
            return;
        }
        
        // Find the next matching closing delimiter
        int next_closer = find_next_closing_delimiter(line.buffer, line.point, c);
        if (next_closer != -1) {
            line.point = next_closer + 1;
            clear_line(&line);
            return;
        }
    }

    // Insert the first character
    memmove(line.buffer + line.point + 1, line.buffer + line.point,
            line.length - line.point);
    line.buffer[line.point] = c;
    line.point++;
    line.length++;

    // Handle electric pair mode
    if (electric_pair_mode && is_opening_delimiter(c)) {
        char matching_char = get_matching_char(c);
        
        // For quotes, only insert pair if we're not already inside quotes
        if ((c == '"' || c == '\'')) {
            int count = 0;
            for (int i = 0; i < line.point - 1; i++) {
                if (line.buffer[i] == c) count++;
            }
            if (count % 2 == 1) return;
        }
        
        // Insert the matching character
        memmove(line.buffer + line.point + 1, line.buffer + line.point,
                line.length - line.point);
        line.buffer[line.point] = matching_char;
        line.length++;
    }

    line.buffer[line.length] = '\0';
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


bool is_matching_pair(char open, char close) {
    return (open == '(' && close == ')')
        || (open == '[' && close == ']')
        || (open == '{' && close == '}')
        || ((open == '"' && close == '"')
            || (open == '\'' && close == '\''));
}


void backward_delete_char() {
    if (line.point > 0) {
        // Check if we're between a matching pair of delimiters
        if (line.point < line.length
            && is_opening_delimiter(line.buffer[line.point - 1])
            && is_matching_pair(line.buffer[line.point - 1], line.buffer[line.point])
            && electric_pair_mode) {
            
            // Delete both characters
            memmove(line.buffer + line.point - 1, 
                    line.buffer + line.point + 1,
                    line.length - line.point - 1);
            line.point--;
            line.length -= 2;
            line.buffer[line.length] = '\0';
        } else {
            // Normal single character deletion
            memmove(line.buffer + line.point - 1, 
                    line.buffer + line.point,
                    line.length - line.point);
            line.point--;
            line.length--;
            line.buffer[line.length] = '\0';
        }
    }
}

void handle_history(int direction) {
    int new_pos = history.current + direction;

    if (new_pos < 0 || new_pos > history.count)
        return;
    if (new_pos == history.count) {
        line.length = 0;
        line.point = 0;
        line.buffer[0] = '\0';
    } else {
        strncpy(line.buffer, history.entries[new_pos], LINE_MAX - 1);
        line.length = strlen(line.buffer);
        line.point = line.length;
    }
    history.current = new_pos;
}


int cd(const char *cmd) {

    if (strcmp(cmd, "cd") == 0) {
        const char *home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "HOME not set\n");
            return 1;
        }
        if (chdir(home) != 0) {
            perror("cd");
            return 1;
        }
        return 0;
    }

    // Skip "cd " to get the path
    const char *path = cmd + 3;

    // Trim leading spaces
    while (*path == ' ') path++;

    // Handle '~' to go to HOME directory
    if (strcmp(path, "~") == 0) {
        path = getenv("HOME");
        if (!path) {
            fprintf(stderr, "HOME not set\n");
            return 1;
        }
    }

    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

int is_builtin(const char *cmd) {
    return strncmp(cmd, "cd ", 3) == 0 || strcmp(cmd, "cd") == 0;
}

int handle_builtin(const char *cmd) {
    if (strncmp(cmd, "cd", 2) == 0) {
        return cd(cmd);
    }
    return 1; // Command not handled
}

void execute_command(const char *cmd) {
    printf("\r");
    fflush(stdout);
    
    // Check for built-in commands first
    if (is_builtin(cmd)) {
        status = handle_builtin(cmd);
        return;
    }
    
    disable_raw_mode();
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        status = 1;
        enable_raw_mode();
        return;
    }
    
    if (pid == 0) {  // Child process
        execlp("/bin/sh", "sh", "-c", cmd, NULL);
        perror("execlp");
        exit(1);
    } else {  // Parent process
        waitpid(pid, &status, 0);
        status = WEXITSTATUS(status);
        enable_raw_mode();
    }
}

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

bool set_system_clipboard(const char *text) {
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
    
    // Write text to pipe
    size_t len = strlen(text);
    ssize_t written = write(pipefd[1], text, len);
    
    close(pipefd[1]);
    
    // Wait for child process
    int status;
    waitpid(pid, &status, 0);
    
    return written == len;
}

// TODO FIX yanking more than one line
void yank(Line *line) {
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


void process_keypress() {
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        switch (c) {

        case 15: //  TODO maybe insert a \n
        case '\r':
        case '\n':
            printf("\n");
            if (line.length > 0) {
                history_add(&line);
                execute_command(line.buffer);
            }
            line.length = 0;
            line.point = 0;
            line.buffer[0] = '\0';
            draw_prompt(status);
            fflush(stdout);
            break;

        case 12:
            clear_screen(&line);
            break;

        case 3: //  TODO send a signal or something and make it cool
            printf("^C\n");
            draw_prompt(status);
            line.length = 0;
            line.point = 0;
            line.buffer[0] = '\0';
            fflush(stdout);
            break;

        case 0: //  
            set_mark(&line);
            break;

        case 25: // 
            yank(&line);
                break;
            // or backward_kill_word if (line->mark == 0)
        case 23: // 
            kill_region(&line);
            break;

        case 11: // 
            kill_line(&line);
            break;

        case 4: // 
            if (line.length == 0) {
                disable_raw_mode();
                raise(SIGINT);
            } else {
                delete_char(&line);
            }
            break;

        case 16:  // 
            handle_history(-1);
            clear_line(&line);
            break;

        case 14:  // 
            handle_history(1);
            clear_line(&line);
            break;

        case 6: // 
            if (line.point < line.length) {
                line.point++;
                clear_line(&line);
            }
            break;

        case 2: // 
            if (line.point > 0) {
                line.point--;
                clear_line(&line);
            }
            break;

        case 127: // 
        case 8: //  / C-Backspace
            backward_delete_char();
            clear_line(&line);
            break;

        case 1: // 
            line.point = 0;
            clear_line(&line);
            break;

        case 5: // 
            line.point = line.length;
            clear_line(&line);
            break;

        case '\x1b':
            if (read(STDIN_FILENO, &c, 1) != 1)
                break;
            if (c != '[')
                break;
            if (read(STDIN_FILENO, &c, 1) != 1)
                break;

            switch (c) {
            case 16:  // 
            case 'A': // 
                handle_history(-1);
                clear_line(&line);
                break;
            case 14:  // 
            case 'B': // 
                handle_history(1);
                clear_line(&line);
                break;
            case 6:   // 
            case 'C': // 
                if (line.point < line.length) {
                    line.point++;
                    clear_line(&line);
                }
                break;
            case 2:   // 
            case 'D': // 
                if (line.point > 0) {
                    line.point--;
                    clear_line(&line);
                }
                break;
            }
            break;

        default:
            if (!iscntrl(c)) {
                insert_char(c);
                clear_line(&line);
            }
            break;
        }
    }
}

int main() {
    enable_raw_mode();
    draw_prompt(status);
    fflush(stdout);

    while (1) {
        process_keypress();
    }

    return 0;
}

