/*
 * File: shell_pipe.h
 * Description:
 * Declarations for the Phase 3 operator and pipeline helpers.
 * Phase 4 still reuses this parsing and execution path for background jobs.
 */

#ifndef SHELL_PIPE_H
#define SHELL_PIPE_H

#include "shell.h"

/* Check whether a full command line contains a Phase 3 operator. */
PipeOperator detect_operator(const char *line);

/* Split a pipeline on '|', and return the number of commands through num_commands. */
char **split_by_pipe(char *line, int *num_commands);

/* Execute a full pipeline such as cmd1 | cmd2 | cmd3. */
void execute_pipeline(char **commands, int num_commands);

/* Execute one command and return its exit status. */
int execute_command(char *command);

/* Execute command chains built with &&, ||, or ;. */
void execute_chained_commands(char *line, PipeOperator op);

/* Only look for operators outside quotes so strings like echo "a | b" stay intact. */
const char *find_operator_outside_quotes(const char *line, const char *op);


#endif
