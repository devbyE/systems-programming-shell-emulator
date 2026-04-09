/*
 * File: shell.h
 * Author: Efrem Wilkerson
 * Description:
 * Header file for the shell project.
 * Defines shared constants and declares the core shell
 * functions used across the program.
 */

#ifndef SHELL_H
#define SHELL_H

#define MAX_ARGS 128
#define MAX_LINE 1024

/* Function prototypes */

void print_prompt(void);

char *read_line(void);

char **parse_line(char *line);

int is_builtin(const char *command);

int execute_builtin_command(char **args);

void execute_external(char **args);

void free_args(char **args);

/* =========================
   Phase 3: Pipe Structures
   ========================= */

typedef enum {
    PIPE_NONE,
    PIPE_BASIC,   // |
    PIPE_AND,     // &&
    PIPE_OR,      // ||
    PIPE_SEQ      // ;
} PipeOperator;

typedef struct {
    char ***commands;      // array of command argument arrays
    PipeOperator *operators;
    int num_commands;
} Pipeline;

#endif