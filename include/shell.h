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

#endif