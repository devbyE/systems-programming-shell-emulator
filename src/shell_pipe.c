/*
 * File: shell_pipe.c
 * Author: Efrem Wilkerson
 * Description:
 * Phase 3 pipe support for the shell.
 * This file detects operators, splits commands by pipe,
 * and executes pipelines using fork, pipe, dup2, and execvp.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "shell.h"

/*
 * detect_operator
 * ----------------
 * Checks the input line for supported Phase 3 operators.
 * Right now this is a simple detector and does not yet
 * handle quote-aware operator detection.
 */
PipeOperator detect_operator(const char *line) {
    if (strstr(line, "&&") != NULL) {
        return PIPE_AND;
    }
    if (strstr(line, "||") != NULL) {
        return PIPE_OR;
    }
    if (strchr(line, '|') != NULL) {
        return PIPE_BASIC;
    }
    if (strchr(line, ';') != NULL) {
        return PIPE_SEQ;
    }
    return PIPE_NONE;
}

/*
 * split_by_pipe
 * -------------
 * Splits a command line into separate command strings
 * using the pipe character '|'.
 *
 * Example:
 *   "echo hello | wc | cat"
 *
 * Becomes:
 *   commands[0] = "echo hello"
 *   commands[1] = "wc"
 *   commands[2] = "cat"
 *
 * num_commands is updated with the number of commands found.
 */
char **split_by_pipe(char *line, int *num_commands) {
    int capacity = 10;
    char **commands = malloc(capacity * sizeof(char *));
    int count = 0;

    char *token = strtok(line, "|");

    while (token != NULL) {
        if (count >= capacity) {
            capacity *= 2;
            commands = realloc(commands, capacity * sizeof(char *));
        }

        /* Trim leading spaces */
        while (*token == ' ') {
            token++;
        }

        commands[count++] = strdup(token);
        token = strtok(NULL, "|");
    }

    *num_commands = count;
    return commands;
}

/*
 * execute_pipeline
 * ----------------
 * Executes multiple commands connected by pipes.
 *
 * Example:
 *   cmd1 | cmd2 | cmd3
 *
 * Strategy:
 *   1. Create all needed pipes
 *   2. Fork one child per command
 *   3. Redirect stdin/stdout with dup2
 *   4. Close all pipe file descriptors
 *   5. execvp each command
 *   6. Parent waits for all children
 */
void execute_pipeline(char **commands, int num_commands) {
    int pipes[num_commands - 1][2];
    pid_t pids[num_commands];

    /* Create all pipes before forking */
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe failed");
            exit(1);
        }
    }

    /* Fork one child process for each command */
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("fork failed");
            exit(1);
        }

        if (pids[i] == 0) {
            /* Child process */

            /* If this is not the first command,
               read input from the previous pipe */
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            /* If this is not the last command,
               send output to the next pipe */
            if (i < num_commands - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            /* Close all pipe file descriptors in the child
               after dup2 redirects what we need */
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            /* Parse this command into argv format */
            char **args = parse_line(commands[i]);

            if (args == NULL || args[0] == NULL) {
                fprintf(stderr, "Invalid command in pipeline\n");
                exit(1);
            }

            /* Replace child with actual command */
            execvp(args[0], args);

            /* Only runs if execvp fails */
            perror("execvp failed");
            exit(1);
        }
    }

    /* Parent closes all pipe ends */
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    /* Parent waits for all child processes */
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

/*
 * execute_command
 * ---------------
 * Executes one normal command and returns its exit status.
 *
 * Return value:
 *   0  -> command succeeded
 *   nonzero -> command failed
 *   -1 -> abnormal termination
 *
 * This is used for Phase 3 logical operators:
 *   &&
 *   ||
 *   ;
 */
int execute_command(char *command) {
    pid_t pid;
    int status;
    char **args;

    args = parse_line(command);

    if (args == NULL || args[0] == NULL) {
        free_args(args);
        return -1;
    }

    /* Handle built-in commands like cd, pwd, exit */
    if (is_builtin(args[0])) {
        status = execute_builtin_command(args);
        free_args(args);
        return status;
    }

    pid = fork();

    if (pid < 0) {
        perror("fork failed");
        free_args(args);
        return -1;
    }

    if (pid == 0) {
        /* Child runs the external command */
        execvp(args[0], args);

        /* Only runs if execvp fails */
        perror("execvp failed");
        free_args(args);
        exit(1);
    }

    /* Parent waits and gets exit status */
    waitpid(pid, &status, 0);
    free_args(args);

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}

/*
 * execute_chained_commands
 * ------------------------
 * Executes commands joined by:
 *   &&
 *   ||
 *   ;
 *
 * Current behavior:
 *   cmd1 && cmd2  -> run cmd2 only if cmd1 succeeds
 *   cmd1 || cmd2  -> run cmd2 only if cmd1 fails
 *   cmd1 ; cmd2   -> always run both
 *
 * This version handles one operator type per command line.
 * Examples it supports:
 *   echo hi && echo yes
 *   false || echo fallback
 *   echo one ; echo two
 */
void execute_chained_commands(char *line, PipeOperator op) {
    char *commands[100];
    int count = 0;
    char *token;
    int last_status = 0;

    if (op == PIPE_AND) {
        token = strtok(line, "&");
        while (token != NULL) {
            while (*token == ' ') {
                token++;
            }

            commands[count++] = token;
            token = strtok(NULL, "&");
        }

        for (int i = 0; i < count; i++) {
            last_status = execute_command(commands[i]);

            if (last_status != 0) {
                break;
            }
        }
    }
    else if (op == PIPE_OR) {
        token = strtok(line, "|");
        while (token != NULL) {
            while (*token == ' ') {
                token++;
            }

            commands[count++] = token;
            token = strtok(NULL, "|");
        }

        for (int i = 0; i < count; i++) {
            last_status = execute_command(commands[i]);

            if (last_status == 0) {
                break;
            }
        }
    }
    else if (op == PIPE_SEQ) {
        token = strtok(line, ";");
        while (token != NULL) {
            while (*token == ' ') {
                token++;
            }

            commands[count++] = token;
            token = strtok(NULL, ";");
        }

        for (int i = 0; i < count; i++) {
            execute_command(commands[i]);
        }
    }
}
