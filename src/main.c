/*
 * File: main.c
 * Author: Efrem Wilkerson
 * Description:
 * Contains the main entry point for the shell.
 * Starts the REPL loop and coordinates command reading,
 * parsing, built-in handling, and external command execution.
 */

#include <stdio.h>
#include <stdlib.h>
#include "../include/shell.h"

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

        if (line[0] == '\0') {
            free(line);
            continue;
        }

        args = parse_line(line);
        free(line);

        if (args == NULL || args[0] == NULL) {
            free_args(args);
            continue;
        }

        if (is_builtin(args[0])) {
            running = execute_builtin_command(args);
        } else {
            execute_external(args);
        }

        free_args(args);
    }

    return 0;
}