/*
 * File: shell_pipe.h
 * Author: Efrem Wilkerson
 * Description:
 * Header file for Phase 3 pipe support.
 * Declares functions used to detect operators,
 * split commands by pipe, execute pipelines,
 * and handle chained commands.
 */

#ifndef SHELL_PIPE_H
#define SHELL_PIPE_H

#include "shell.h"

/*
 * Checks a command line for supported Phase 3 operators.
 */
PipeOperator detect_operator(const char *line);

/*
 * Splits a command line into separate commands using '|'.
 * Stores the number of commands found in num_commands.
 */
char **split_by_pipe(char *line, int *num_commands);

/*
 * Executes commands connected by pipes.
 * Example: cmd1 | cmd2 | cmd3
 */
void execute_pipeline(char **commands, int num_commands);

/*
 * Executes one normal command and returns its exit status.
 */
int execute_command(char *command);

/*
 * Executes commands joined by:
 *   &&
 *   ||
 *   ;
 */
void execute_chained_commands(char *line, PipeOperator op);

#endif
