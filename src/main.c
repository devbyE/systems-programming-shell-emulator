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

        /* --------------------------------------------------
         * Phase 3 Operator Handling
         * --------------------------------------------------
         * IMPORTANT:
         * Mixed expressions like:
         *   echo "a" && echo "b" ; echo "c"
         * must let ';' split the line first.
         * So we check for ';' before detect_operator().
         */

        /* Handle sequential commands first */
        if (find_operator_outside_quotes(original_line, ";") != NULL) {
            char *line_copy = strdup(original_line);
            execute_chained_commands(line_copy, PIPE_SEQ);
            free(line_copy);
        }

        else {
            PipeOperator op = detect_operator(original_line);

            /* Handle && and || */
            if (op == PIPE_AND || op == PIPE_OR) {
                char *line_copy = strdup(original_line);
                execute_chained_commands(line_copy, op);
                free(line_copy);
            }

            /* Handle pipelines */
            else if (op == PIPE_BASIC) {
                int num_commands = 0;

                /* strtok modifies string, so use a copy */
                char *line_copy = strdup(original_line);

                char **commands = split_by_pipe(line_copy, &num_commands);
                execute_pipeline(commands, num_commands);

                /* cleanup */
                for (int i = 0; i < num_commands; i++) {
                    free(commands[i]);
                }
                free(commands);
                free(line_copy);
            }

            /* Handle normal commands (Phase 1/2 behavior) */
            else {
                args = parse_line(line);

                if (args == NULL || args[0] == NULL) {
                    free_args(args);
                    free(original_line);
                    free(line);
                    continue;
                }

                /* Built-in commands */
                if (is_builtin(args[0])) {
                    running = execute_builtin_command(args);
                }
                else {
                    /* External command */
                    execute_external(args);
                }

                free_args(args);
            }
        }

        free(original_line);
        free(line);
    }

    return 0;
}