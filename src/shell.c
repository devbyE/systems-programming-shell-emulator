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

#include <ctype.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../include/shell.h"
#include "../include/redirect.h"

static int last_quote_flags[MAX_ARGS];

static char *duplicate_string(const char *source)
{
    char *copy;

    if (source == NULL) {
        return NULL;
    }

    copy = malloc(strlen(source) + 1);
    if (copy == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return NULL;
    }

    strcpy(copy, source);
    return copy;
}

static int append_argument(char **args, int *count, const char *token, int quoted)
{
    args[*count] = duplicate_string(token);
    if (args[*count] == NULL) {
        return -1;
    }

    last_quote_flags[*count] = quoted;
    (*count)++;
    args[*count] = NULL;
    return 0;
}

static int flush_token(char **args, int *count, char *token, int *length,
                       int *quoted)
{
    if (*length == 0 && *quoted == 0) {
        return 0;
    }

    token[*length] = '\0';

    if (*count >= MAX_ARGS - 1) {
        fprintf(stderr, "myshell: too many arguments\n");
        return -1;
    }

    if (append_argument(args, count, token, *quoted) != 0) {
        return -1;
    }

    *length = 0;
    *quoted = 0;
    return 0;
}

static int add_special_token(char **args, int *count, const char *token)
{
    if (*count >= MAX_ARGS - 1) {
        fprintf(stderr, "myshell: too many arguments\n");
        return -1;
    }

    if (append_argument(args, count, token, 0) != 0) {
        return -1;
    }

    return 0;
}

static int append_token_char(char *token, int *length, char character)
{
    if (*length >= MAX_LINE - 1) {
        fprintf(stderr, "myshell: input line too long\n");
        return -1;
    }

    token[*length] = character;
    (*length)++;
    return 0;
}

static int has_glob_characters(const char *arg)
{
    return arg != NULL && strpbrk(arg, "*?[") != NULL;
}

static int push_expanded_arg(char ***expanded_args, size_t *count, size_t *capacity,
                             const char *value)
{
    char **resized_args;

    if (*count + 1 >= *capacity) {
        *capacity *= 2;
        resized_args = realloc(*expanded_args, *capacity * sizeof(char *));
        if (resized_args == NULL) {
            fprintf(stderr, "Memory allocation error\n");
            return -1;
        }
        *expanded_args = resized_args;
    }

    (*expanded_args)[*count] = duplicate_string(value);
    if ((*expanded_args)[*count] == NULL) {
        return -1;
    }

    (*count)++;
    (*expanded_args)[*count] = NULL;
    return 0;
}

static char **expand_globs(char **args)
{
    char **expanded_args;
    size_t count = 0;
    size_t capacity = 8;
    int i;

    expanded_args = malloc(capacity * sizeof(char *));
    if (expanded_args == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return NULL;
    }

    expanded_args[0] = NULL;

    for (i = 0; args[i] != NULL; i++) {
        glob_t matches;
        int glob_status;
        size_t j;

        if (last_quote_flags[i] != 0 || !has_glob_characters(args[i])) {
            if (push_expanded_arg(&expanded_args, &count, &capacity, args[i]) != 0) {
                free_args(expanded_args);
                return NULL;
            }
            continue;
        }

        memset(&matches, 0, sizeof(matches));
        glob_status = glob(args[i], 0, NULL, &matches);

        if (glob_status == 0) {
            for (j = 0; j < matches.gl_pathc; j++) {
                if (push_expanded_arg(&expanded_args, &count, &capacity,
                                      matches.gl_pathv[j]) != 0) {
                    globfree(&matches);
                    free_args(expanded_args);
                    return NULL;
                }
            }
            globfree(&matches);
            continue;
        }

        globfree(&matches);

        if (glob_status != GLOB_NOMATCH) {
            fprintf(stderr, "myshell: wildcard expansion failed for %s\n", args[i]);
            free_args(expanded_args);
            return NULL;
        }

        if (push_expanded_arg(&expanded_args, &count, &capacity, args[i]) != 0) {
            free_args(expanded_args);
            return NULL;
        }
    }

    return expanded_args;
}

static int save_standard_streams(int saved_streams[3])
{
    int stream_ids[3] = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
    int i;

    for (i = 0; i < 3; i++) {
        saved_streams[i] = dup(stream_ids[i]);
        if (saved_streams[i] < 0) {
            fprintf(stderr, "myshell: failed to save standard streams\n");
            while (--i >= 0) {
                close(saved_streams[i]);
            }
            return -1;
        }
    }

    return 0;
}

static void restore_standard_streams(int saved_streams[3])
{
    int stream_ids[3] = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
    int i;

    for (i = 0; i < 3; i++) {
        if (saved_streams[i] >= 0) {
            dup2(saved_streams[i], stream_ids[i]);
            close(saved_streams[i]);
        }
    }
}

static int run_builtin_command(char **args)
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
    char **args = calloc(MAX_ARGS, sizeof(char *));
    char token[MAX_LINE];
    int arg_count = 0;
    int token_length = 0;
    int token_quoted = 0;
    int in_single_quotes = 0;
    int in_double_quotes = 0;
    int i = 0;

    if (args == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return NULL;
    }

    memset(last_quote_flags, 0, sizeof(last_quote_flags));

    while (line[i] != '\0') {
        if (!in_single_quotes && !in_double_quotes &&
            isspace((unsigned char)line[i])) {
            if (flush_token(args, &arg_count, token, &token_length,
                            &token_quoted) != 0) {
                free_args(args);
                return NULL;
            }
            i++;
            continue;
        }

        if (!in_single_quotes && line[i] == '"') {
            in_double_quotes = !in_double_quotes;
            token_quoted = 1;
            i++;
            continue;
        }

        if (!in_double_quotes && line[i] == '\'') {
            in_single_quotes = !in_single_quotes;
            token_quoted = 1;
            i++;
            continue;
        }

        if (!in_single_quotes && line[i] == '\\' && line[i + 1] != '\0') {
            token_quoted = 1;
            if (append_token_char(token, &token_length, line[i + 1]) != 0) {
                free_args(args);
                return NULL;
            }
            i += 2;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes) {
            if (line[i] == '2' && line[i + 1] == '>' && token_length == 0) {
                if (flush_token(args, &arg_count, token, &token_length,
                                &token_quoted) != 0 ||
                    add_special_token(args, &arg_count, "2>") != 0) {
                    free_args(args);
                    return NULL;
                }
                i += 2;
                continue;
            }

            if (line[i] == '>') {
                if (flush_token(args, &arg_count, token, &token_length,
                                &token_quoted) != 0) {
                    free_args(args);
                    return NULL;
                }

                if (line[i + 1] == '>') {
                    if (add_special_token(args, &arg_count, ">>") != 0) {
                        free_args(args);
                        return NULL;
                    }
                    i += 2;
                } else {
                    if (add_special_token(args, &arg_count, ">") != 0) {
                        free_args(args);
                        return NULL;
                    }
                    i++;
                }
                continue;
            }

            if (line[i] == '<') {
                if (flush_token(args, &arg_count, token, &token_length,
                                &token_quoted) != 0 ||
                    add_special_token(args, &arg_count, "<") != 0) {
                    free_args(args);
                    return NULL;
                }
                i++;
                continue;
            }
        }

        if (append_token_char(token, &token_length, line[i]) != 0) {
            free_args(args);
            return NULL;
        }
        i++;
    }

    if (in_single_quotes || in_double_quotes) {
        fprintf(stderr, "myshell: unmatched quote\n");
        free_args(args);
        return NULL;
    }

    if (flush_token(args, &arg_count, token, &token_length, &token_quoted) != 0) {
        free_args(args);
        return NULL;
    }

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
    char **expanded_args;
    int saved_streams[3] = {-1, -1, -1};
    int status = 1;

    expanded_args = expand_globs(args);
    if (expanded_args == NULL) {
        return 1;
    }

    if (save_standard_streams(saved_streams) != 0) {
        free_args(expanded_args);
        return 1;
    }

    if (setup_redirection(expanded_args) != 0) {
        restore_standard_streams(saved_streams);
        free_args(expanded_args);
        return 1;
    }

    if (expanded_args[0] != NULL) {
        status = run_builtin_command(expanded_args);
    }

    fflush(stdout);
    fflush(stderr);
    restore_standard_streams(saved_streams);
    free_args(expanded_args);
    return status;
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
        char **expanded_args = expand_globs(args);

        if (expanded_args == NULL) {
            exit(EXIT_FAILURE);
        }

        if (setup_redirection(expanded_args) != 0) {
            free_args(expanded_args);
            exit(EXIT_FAILURE);
        }

        if (expanded_args[0] == NULL) {
            free_args(expanded_args);
            exit(EXIT_SUCCESS);
        }

        execvp(expanded_args[0], expanded_args);
        fprintf(stderr, "myshell: command not found: %s\n", expanded_args[0]);
        free_args(expanded_args);
        exit(EXIT_FAILURE);
    } else {
        if (waitpid(pid, &status, 0) == -1) {
            fprintf(stderr, "myshell: waitpid failed\n");
        }
    }
}

