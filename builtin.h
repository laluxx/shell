#ifndef BUILTIN_H
#define BUILTIN_H

int is_builtin(const char *cmd);
int handle_builtin(const char *cmd);

int cd(const char *cmd);
int colors();

#endif
