#ifndef ELECTRIC_PAIR_MODE_H
#define ELECTRIC_PAIR_MODE_H

#include <stdbool.h>

extern bool electric_pair_mode;

bool is_opening_delimiter(char c);
bool is_closing_delimiter(char c);
char get_matching_char(char c);
int find_next_closing_delimiter(const char *buffer, int point, char c);
bool is_inside_delimiters(const char *buffer, int point);
bool is_matching_pair(char open, char close);

#endif
