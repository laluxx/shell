#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include "ansi_codes.h"
#include "line.h"
#include "prompt.h"
#include "completion.h"
#include "history.h"
#include "electric_pair_mode.h"
#include "clipboard.h"
#include "builtin.h"

History h = {0};
Line line = {0};
char *clipboard = NULL;
struct termios orig_termios;


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
        /* execlp("/bin/python3", "python3", "-c", cmd, NULL); */
        /* execlp("/bin/nu", "nu", "-c", cmd, NULL); */
        perror("execlp");
        exit(1);
    } else {  // Parent process
        waitpid(pid, &status, 0);
        status = WEXITSTATUS(status);
        enable_raw_mode();
    }
}

void process_keypress() {
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        switch (c) {

        case '\t': {
            handle_completion(&line);
            break;
        }
            
        case 15: //  TODO
            printf("\n");
            break;

        case '\r':
        case '\n':
            printf("\n");
            if (line.length > 0) {
                history_add(&h, &line);
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
            yank(&line, clipboard);
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
            handle_history(&h, &line, -1);
            clear_line(&line);
            break;

        case 14:  // 
            handle_history(&h, &line, 1);
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
            backward_delete_char(&line);
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
            case 'A':
                handle_history(&h, &line, -1);
                clear_line(&line);
                break;
            case 14:  // 
            case 'B':
                handle_history(&h, &line, 1);
                clear_line(&line);
                break;
            case 6:   // 
            case 'C':
                if (line.point < line.length) {
                    line.point++;
                    clear_line(&line);
                }
                break;
            case 2:   // 
            case 'D':
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
    init_command_cache();
    draw_prompt(status);
    fflush(stdout);

    while (1) {
        process_keypress();
    }

    return 0;
}

