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
#include "redirect.h"
#include "job_control.h"

/*
 * find_operator_outside_quotes
 * ----------------------------
 * Searches for an operator in the input string, but only if that operator
 * appears outside of single quotes or double quotes.
 */
const char *find_operator_outside_quotes(const char *line, const char *op) {
    int in_single_quote = 0;
    int in_double_quote = 0;
    size_t op_len = strlen(op);

    for (const char *ptr = line; *ptr != '\0'; ptr++) {
        if (*ptr == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        }
        else if (*ptr == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        }
        else if (!in_single_quote && !in_double_quote) {
            if (strncmp(ptr, op, op_len) == 0) {
                return ptr;
            }
        }
    }

    return NULL;
}

/*
 * detect_operator
 * ----------------
 * Checks the input line for supported Phase 3 operators.
 * This version only detects operators that appear outside quotes.
 */
PipeOperator detect_operator(const char *line) {
    if (find_operator_outside_quotes(line, "&&") != NULL) {
        return PIPE_AND;
    }
    if (find_operator_outside_quotes(line, "||") != NULL) {
        return PIPE_OR;
    }
    if (find_operator_outside_quotes(line, "|") != NULL) {
        return PIPE_BASIC;
    }
    if (find_operator_outside_quotes(line, ";") != NULL) {
        return PIPE_SEQ;
    }
    return PIPE_NONE;
}

/*
 * split_by_pipe
 * -------------
 * Splits a command line into separate command strings
 * using the pipe character '|', but only outside quotes.
 */
char **split_by_pipe(char *line, int *num_commands) {
    int capacity = 10;
    char **commands = malloc(capacity * sizeof(char *));
    int count = 0;

    int in_single_quote = 0;
    int in_double_quote = 0;
    char *start = line;

    for (char *ptr = line; ; ptr++) {
        if (*ptr == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        }
        else if (*ptr == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        }

        if (((*ptr == '|') && !in_single_quote && !in_double_quote) || *ptr == '\0') {
            char saved = *ptr;
            *ptr = '\0';

            while (*start == ' ') {
                start++;
            }

            if (count >= capacity) {
                capacity *= 2;
                commands = realloc(commands, capacity * sizeof(char *));
            }

            commands[count++] = strdup(start);

            if (saved == '\0') {
                break;
            }

            start = ptr + 1;
        }
    }

    *num_commands = count;
    return commands;
}

/*
 * split_by_operator
 * -----------------
 * Splits a line by a specific operator only when that operator
 * appears outside quotes.
 *
 * Supports:
 *   &&
 *   ||
 *   ;
 */
static int split_by_operator(char *line, const char *op, char **commands, int max_commands) {
    int count = 0;
    int in_single_quote = 0;
    int in_double_quote = 0;
    size_t op_len = strlen(op);
    char *start = line;

    for (char *ptr = line; ; ptr++) {
        if (*ptr == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
        }
        else if (*ptr == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
        }

        if (((*ptr != '\0') &&
             !in_single_quote &&
             !in_double_quote &&
             strncmp(ptr, op, op_len) == 0) ||
            *ptr == '\0') {

            char saved0 = *ptr;
            char saved1 = '\0';

            if (*ptr != '\0') {
                *ptr = '\0';
                if (op_len == 2) {
                    saved1 = *(ptr + 1);
                    *(ptr + 1) = '\0';
                }
            }

            while (*start == ' ') {
                start++;
            }

            if (count < max_commands) {
                commands[count++] = start;
            }

            if (saved0 == '\0') {
                break;
            }

            if (op_len == 2) {
                *(ptr + 1) = saved1;
            }

            start = ptr + op_len;
        }
    }

    return count;
}

/*
 * execute_pipeline_status
 * -----------------------
 * Executes multiple commands connected by pipes and returns
 * the exit status of the last command in the pipeline.
 */
static int execute_pipeline_status(char **commands, int num_commands) {
    int pipes[num_commands - 1][2];
    pid_t pids[num_commands];
    pid_t pgid = 0;
    int status = 0;
    int last_status = 0;
    char command_text[MAX_LINE] = {0};

    for (int i = 0; i < num_commands; i++) {
        if (i > 0 && strlen(command_text) < MAX_LINE - 1) {
            strncat(command_text, " | ", MAX_LINE - strlen(command_text) - 1);
        }
        strncat(command_text, commands[i], MAX_LINE - strlen(command_text) - 1);
    }

    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe failed");
            return -1;
        }
    }

    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("fork failed");
            return -1;
        }

        if (pids[i] == 0) {
            if (pgid == 0) {
                setpgid(0, 0);
            }
            else {
                setpgid(0, pgid);
            }

            restore_default_job_signals();

            /* If this is not the first command, read from previous pipe */
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            /* If this is not the last command, write to next pipe */
            if (i < num_commands - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            /* Close all pipe ends in child */
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            char **args = parse_line(commands[i]);
            char **expanded_args;

            if (args == NULL || args[0] == NULL) {
                fprintf(stderr, "Invalid command in pipeline\n");
                free_args(args);
                exit(1);
            }

            expanded_args = expand_glob_patterns(args);
            free_args(args);

            if (expanded_args == NULL || expanded_args[0] == NULL) {
                free_args(expanded_args);
                exit(1);
            }

            if (setup_redirection(expanded_args) != 0) {
                free_args(expanded_args);
                exit(1);
            }

            if (is_builtin(expanded_args[0])) {
                int builtin_status = execute_builtin_command(expanded_args);
                free_args(expanded_args);
                exit(builtin_status);
            }

            execvp(expanded_args[0], expanded_args);
            fprintf(stderr, "myshell: command not found: %s\n", expanded_args[0]);
            free_args(expanded_args);
            exit(1);
        }

        if (pgid == 0) {
            pgid = pids[i];
        }
        setpgid(pids[i], pgid);
    }

    /* Parent closes all pipe ends */
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    set_foreground_pgid(pgid);

    /* Wait in command order so we can capture the last command's status */
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], &status, WUNTRACED);

        if (WIFSTOPPED(status)) {
            clear_foreground_pgid();
            register_stopped_foreground_job(pgid, command_text, status);
            return 128 + WSTOPSIG(status);
        }

        if (i == num_commands - 1) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            }
            else {
                last_status = -1;
            }
        }
    }

    clear_foreground_pgid();
    return last_status;
}

/*
 * execute_pipeline
 * ----------------
 * Executes multiple commands connected by pipes.
 */
void execute_pipeline(char **commands, int num_commands) {
    (void) execute_pipeline_status(commands, num_commands);
}

/*
 * Forward declaration
 * -------------------
 * Lets execute_command call execute_chained_commands_status
 * before its full definition appears later in the file.
 */
static int execute_chained_commands_status(char *line, PipeOperator op);

/*
 * execute_command
 * ---------------
 * Executes one command, chained command, or pipeline
 * and returns exit status.
 */
int execute_command(char *command) {
    pid_t pid;
    int status;
    char **args;
    char **expanded_args;
    char command_text[MAX_LINE] = {0};

    /* Handle sequential commands inside command execution */
    if (find_operator_outside_quotes(command, ";") != NULL) {
        char *copy = strdup(command);
        int last_status = execute_chained_commands_status(copy, PIPE_SEQ);
        free(copy);
        return last_status;
    }

    PipeOperator op = detect_operator(command);

    /* Handle && and || inside command segments */
    if (op == PIPE_AND || op == PIPE_OR) {
        char *copy = strdup(command);
        int last_status = execute_chained_commands_status(copy, op);
        free(copy);
        return last_status;
    }

    /* Handle pipeline inside chained commands */
    if (op == PIPE_BASIC) {
        int num_commands = 0;
        char *copy = strdup(command);
        int last_status;

        char **cmds = split_by_pipe(copy, &num_commands);
        last_status = execute_pipeline_status(cmds, num_commands);

        for (int i = 0; i < num_commands; i++) {
            free(cmds[i]);
        }
        free(cmds);
        free(copy);

        return last_status;
    }

    args = parse_line(command);

    if (!args || !args[0]) {
        free_args(args);
        return -1;
    }

    if (is_builtin(args[0])) {
        status = execute_builtin_with_redirection(args);
        free_args(args);
        return status;
    }

    expanded_args = expand_glob_patterns(args);
    free_args(args);

    if (expanded_args == NULL || expanded_args[0] == NULL) {
        free_args(expanded_args);
        return -1;
    }

    for (int i = 0; expanded_args[i] != NULL; i++) {
        if (i > 0 && strlen(command_text) < MAX_LINE - 1) {
            strncat(command_text, " ", MAX_LINE - strlen(command_text) - 1);
        }
        strncat(command_text, expanded_args[i], MAX_LINE - strlen(command_text) - 1);
    }

    pid = fork();

    if (pid < 0) {
        perror("fork failed");
        free_args(expanded_args);
        return -1;
    }

    if (pid == 0) {
        setpgid(0, 0);
        restore_default_job_signals();

        if (setup_redirection(expanded_args) != 0) {
            free_args(expanded_args);
            exit(1);
        }

        execvp(expanded_args[0], expanded_args);
        fprintf(stderr, "myshell: command not found: %s\n", expanded_args[0]);
        free_args(expanded_args);
        exit(1);
    }

    setpgid(pid, pid);
    status = wait_for_foreground_job(pid, command_text);
    free_args(expanded_args);

    return status;
}

/*
 * execute_chained_commands_status
 * -------------------------------
 * Executes commands joined by &&, ||, ;
 * and returns the final exit status.
 */
static int execute_chained_commands_status(char *line, PipeOperator op) {
    char *commands[100];
    int count = 0;
    int last_status = 0;

    if (op == PIPE_AND) {
        count = split_by_operator(line, "&&", commands, 100);

        for (int i = 0; i < count; i++) {
            last_status = execute_command(commands[i]);

            if (last_status != 0) {
                break;
            }
        }

        return last_status;
    }

    if (op == PIPE_OR) {
        count = split_by_operator(line, "||", commands, 100);

        for (int i = 0; i < count; i++) {
            last_status = execute_command(commands[i]);

            if (last_status == 0) {
                break;
            }
        }

        return last_status;
    }

    if (op == PIPE_SEQ) {
        count = split_by_operator(line, ";", commands, 100);

        for (int i = 0; i < count; i++) {
            last_status = execute_command(commands[i]);
        }

        return last_status;
    }

    return execute_command(line);
}

/*
 * execute_chained_commands
 * ------------------------
 * Executes commands joined by &&, ||, ;
 */
void execute_chained_commands(char *line, PipeOperator op) {
    (void) execute_chained_commands_status(line, op);
}
