/*
 * File: main.c
 * Author: Efrem Wilkerson
 * Description:
 * Main shell loop.
 * Phase 3 update: Added operator detection, pipeline execution,
 * and chained command execution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/shell.h"
#include "../include/shell_pipe.h"

int main(void)
{
    char *line;
    char **args;
    int running = 1;

    while (running) {
        print_prompt();

        line = read_line();

        if (line == NULL) {
            printf("\n");
            break;
        }

        /* Skip empty input lines */
        if (line[0] == '\0') {
            free(line);
            continue;
        }

        /* Keep a copy of the original input for Phase 3 parsing */
        char *original_line = strdup(line);

        /* Detect whether the line contains a Phase 3 operator */
        PipeOperator op = detect_operator(original_line);

        /* Handle pipe-based commands */
        if (op == PIPE_BASIC) {
            int num_commands = 0;

            /* Make a modifiable copy because strtok changes the string */
            char *line_copy = strdup(original_line);

            /* Split the command into pieces around | */
            char **commands = split_by_pipe(line_copy, &num_commands);

            /* Execute the full pipeline */
            execute_pipeline(commands, num_commands);

            /* Free split command strings */
            for (int i = 0; i < num_commands; i++) {
                free(commands[i]);
            }
            free(commands);
            free(line_copy);
        }
        /* Handle &&, ||, and ; */
        else if (op == PIPE_AND || op == PIPE_OR || op == PIPE_SEQ) {
            char *line_copy = strdup(original_line);
            execute_chained_commands(line_copy, op);
            free(line_copy);
        }
        else {
            /* Phase 1 / Phase 2 normal command path */
            args = parse_line(line);

            if (args == NULL || args[0] == NULL) {
                free_args(args);
                free(original_line);
                free(line);
                continue;
            }

            /* Built-in commands like cd, pwd, exit */
            if (is_builtin(args[0])) {
                running = execute_builtin_command(args);
            }
            else {
                /* External commands */
                execute_external(args);
            }

            free_args(args);
        }

        free(original_line);
        free(line);
    }

    return 0;
}
