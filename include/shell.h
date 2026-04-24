/*
 * File: shell.h
 * Description:
 * Shared constants and core shell declarations live here.
 * Phase 4 hooks into this header too, so execution entry points,
 * glob expansion, and job-control related declarations all meet here.
 */

#ifndef SHELL_H
#define SHELL_H

#define MAX_ARGS 128
#define MAX_LINE 1024
#define MAX_JOBS 128

/* Main parsing and execution entry points. */

void print_prompt(void);

char *read_line(void);

char **parse_line(char *line);

char **expand_glob_patterns(char **args);

int is_builtin(const char *command);

int execute_builtin_command(char **args);

int execute_builtin_with_redirection(char **args);

int execute_external(char **args);

int execute_line(char *line, int *running);

void free_args(char **args);

/* Phase 3 operator types still drive Phase 4 command execution as well. */

typedef enum {
    PIPE_NONE,
    PIPE_BASIC,   // |
    PIPE_AND,     // &&
    PIPE_OR,      // ||
    PIPE_SEQ      // ;
} PipeOperator;

typedef struct {
    char ***commands;      // argument array for each command
    PipeOperator *operators;
    int num_commands;
} Pipeline;

#endif
