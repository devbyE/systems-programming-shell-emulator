/*
 * File: shell.c
 * Author: Efrem Wilkerson
 * Description:
 * Implements the main shell loop.
 * Handles reading user input, parsing arguments,
 * creating child processes, and executing commands.
 * Phase 2 adds support for input/output/error redirection.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../include/shell.h"
#include "../include/redirect.h"

void print_prompt(void)
{
    printf("myshell> ");
    fflush(stdout);
}

char *read_line(void)
{
    char buffer[MAX_LINE];

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NULL;
    }

    buffer[strcspn(buffer, "\n")] = '\0';

    char *line = malloc(strlen(buffer) + 1);
    if (line == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return NULL;
    }

    strcpy(line, buffer);
    return line;
}

char **parse_line(char *line)
{
    char **args = malloc(MAX_ARGS * sizeof(char *));
    char *line_copy;
    char *token;
    int i = 0;

    if (args == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return NULL;
    }

    line_copy = malloc(strlen(line) + 1);
    if (line_copy == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        free(args);
        return NULL;
    }

    strcpy(line_copy, line);

    token = strtok(line_copy, " \t\n");

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i] = malloc(strlen(token) + 1);
        if (args[i] == NULL) {
            fprintf(stderr, "Memory allocation error\n");
            free(line_copy);
            free_args(args);
            return NULL;
        }

        strcpy(args[i], token);
        i++;
        token = strtok(NULL, " \t\n");
    }

    args[i] = NULL;
    free(line_copy);
    return args;
}

void free_args(char **args)
{
    int i;

    if (args == NULL) {
        return;
    }

    for (i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }

    free(args);
}

int is_builtin(const char *command)
{
    if (command == NULL) {
        return 0;
    }

    if (strcmp(command, "exit") == 0 ||
        strcmp(command, "cd") == 0 ||
        strcmp(command, "pwd") == 0) {
        return 1;
    }

    return 0;
}

int execute_builtin_command(char **args)
{
    char cwd[MAX_LINE];

    if (args == NULL || args[0] == NULL) {
        return 1;
    }

    if (strcmp(args[0], "exit") == 0) {
        printf("Goodbye\n");
        return 0;
    }

    if (strcmp(args[0], "cd") == 0) {
        char *path = args[1];

        if (path == NULL) {
            path = getenv("HOME");
        }

        if (path == NULL || chdir(path) != 0) {
            fprintf(stderr, "myshell: cd: %s: No such file or directory\n",
                    path ? path : "");
        }

        return 1;
    }

    if (strcmp(args[0], "pwd") == 0) {
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            fprintf(stderr, "myshell: pwd: error retrieving current directory\n");
        } else {
            printf("%s\n", cwd);
        }

        return 1;
    }

    return 1;
}

void execute_external(char **args)
{
    pid_t pid;
    int status;

    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "myshell: fork failed\n");
        return;
    }

    if (pid == 0) {
        /* Phase 2:
       Apply input/output/error redirection before executing command */
    if (setup_redirection(args) != 0) {
        exit(EXIT_FAILURE);
    }
    /* Execute command after redirection is set up */
    execvp(args[0], args);
        fprintf(stderr, "myshell: command not found: %s\n", args[0]);
        exit(EXIT_FAILURE);
    } else {
        if (waitpid(pid, &status, 0) == -1) {
            fprintf(stderr, "myshell: waitpid failed\n");
        }
    }
}

