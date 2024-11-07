#include "electric_pair_mode.h"

// TODO implement forward_list() and backward_list()

bool electric_pair_mode = true;

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

bool is_matching_pair(char open, char close) {
    return (open == '(' && close == ')')
        || (open == '[' && close == ']')
        || (open == '{' && close == '}')
        || ((open == '"' && close == '"')
            || (open == '\'' && close == '\''));
}

