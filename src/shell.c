/*
 * File: shell.c
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
#include "../include/job_control.h"
#include "../include/shell_pipe.h"

void print_prompt(void)
{
    if (isatty(STDIN_FILENO)) {
        printf("myshell> ");
        fflush(stdout);
    }
}

char *read_line(void)
{
    char buffer[MAX_LINE];
    char *line = NULL;
    size_t line_length = 0;
    int in_single_quote = 0;
    int in_double_quote = 0;

    while (1) {
        size_t chunk_length;
        char *new_line;

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            if (line == NULL) {
                return NULL;
            }
            break;
        }

        chunk_length = strcspn(buffer, "\n");
        buffer[chunk_length] = '\0';

        new_line = realloc(line, line_length + chunk_length + 2);
        if (new_line == NULL) {
            fprintf(stderr, "Memory allocation error\n");
            free(line);
            return NULL;
        }

        line = new_line;
        memcpy(line + line_length, buffer, chunk_length);
        line_length += chunk_length;
        line[line_length] = '\0';

        for (size_t i = 0; i < chunk_length; i++) {
            if (buffer[i] == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
            }
            else if (buffer[i] == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
            }
        }

        if (!in_single_quote && !in_double_quote) {
            break;
        }

        line[line_length++] = '\n';
        line[line_length] = '\0';
    }

    return line;
}

static int special_token_length(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    if ((text[0] == '&' && text[1] == '&') ||
        (text[0] == '|' && text[1] == '|') ||
        (text[0] == '>' && text[1] == '>') ||
        (text[0] == '2' && text[1] == '>')) {
        return 2;
    }

    if (text[0] == '<' || text[0] == '>' || text[0] == '|' || text[0] == ';') {
        return 1;
    }

    return 0;
}

static char *duplicate_range(const char *start, size_t length)
{
    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static int has_glob_metachar(const char *text)
{
    if (text == NULL) {
        return 0;
    }

    return strchr(text, '*') != NULL ||
           strchr(text, '?') != NULL ||
           strchr(text, '[') != NULL;
}

char **expand_glob_patterns(char **args)
{
    char **expanded = calloc(MAX_ARGS, sizeof(char *));
    int out = 0;

    if (expanded == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return NULL;
    }

    for (int i = 0; args[i] != NULL && out < MAX_ARGS - 1; i++) {
        glob_t matches;

        if (!has_glob_metachar(args[i])) {
            expanded[out] = strdup(args[i]);
            if (expanded[out] == NULL) {
                free_args(expanded);
                return NULL;
            }
            out++;
            continue;
        }

        memset(&matches, 0, sizeof(matches));
        if (glob(args[i], 0, NULL, &matches) == 0) {
            for (size_t j = 0; j < matches.gl_pathc && out < MAX_ARGS - 1; j++) {
                expanded[out] = strdup(matches.gl_pathv[j]);
                if (expanded[out] == NULL) {
                    globfree(&matches);
                    free_args(expanded);
                    return NULL;
                }
                out++;
            }
            globfree(&matches);
        }
        else {
            expanded[out] = strdup(args[i]);
            if (expanded[out] == NULL) {
                free_args(expanded);
                return NULL;
            }
            out++;
        }
    }

    expanded[out] = NULL;
    return expanded;
}

char **parse_line(char *line)
{
    char **args = calloc(MAX_ARGS, sizeof(char *));
    int count = 0;
    size_t i = 0;

    if (args == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return NULL;
    }

    while (line[i] != '\0' && count < MAX_ARGS - 1) {
        char token[MAX_LINE];
        int token_len = 0;
        int in_single_quote = 0;
        int in_double_quote = 0;
        int token_started = 0;
        int op_len = 0;

        while (line[i] != '\0' && isspace((unsigned char) line[i])) {
            i++;
        }

        if (line[i] == '\0') {
            break;
        }

        op_len = special_token_length(&line[i]);
        if (op_len > 0) {
            args[count] = duplicate_range(&line[i], (size_t) op_len);
            if (args[count] == NULL) {
                fprintf(stderr, "Memory allocation error\n");
                free_args(args);
                return NULL;
            }

            count++;
            i += (size_t) op_len;
            continue;
        }

        while (line[i] != '\0') {
            if (line[i] == '\'' && !in_double_quote) {
                in_single_quote = !in_single_quote;
                token_started = 1;
                i++;
                continue;
            }

            if (line[i] == '"' && !in_single_quote) {
                in_double_quote = !in_double_quote;
                token_started = 1;
                i++;
                continue;
            }

            if (!in_single_quote && !in_double_quote) {
                if (isspace((unsigned char) line[i])) {
                    break;
                }

                op_len = special_token_length(&line[i]);
                if (op_len > 0) {
                    break;
                }
            }

            token[token_len++] = line[i];
            token_started = 1;
            i++;
        }

        token[token_len] = '\0';

        if (token_started) {
            args[count] = strdup(token);
            if (args[count] == NULL) {
                fprintf(stderr, "Memory allocation error\n");
                free_args(args);
                return NULL;
            }
            count++;
        }
    }

    args[count] = NULL;
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
        strcmp(command, "pwd") == 0 ||
        strcmp(command, "jobs") == 0 ||
        strcmp(command, "fg") == 0 ||
        strcmp(command, "bg") == 0 ||
        strcmp(command, "wait") == 0) {
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
            return 1;
        }

        return 0;
    }

    if (strcmp(args[0], "pwd") == 0) {
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            fprintf(stderr, "myshell: pwd: error retrieving current directory\n");
            return 1;
        } else {
            printf("%s\n", cwd);
        }

        return 0;
    }

    if (strcmp(args[0], "jobs") == 0) {
        return jobs_builtin(args);
    }

    if (strcmp(args[0], "fg") == 0) {
        return fg_builtin(args);
    }

    if (strcmp(args[0], "bg") == 0) {
        return bg_builtin(args);
    }

    if (strcmp(args[0], "wait") == 0) {
        return wait_builtin(args);
    }

    return 1;
}

int execute_builtin_with_redirection(char **args)
{
    int saved_stdin;
    int saved_stdout;
    int saved_stderr;
    int status;

    saved_stdin = dup(STDIN_FILENO);
    saved_stdout = dup(STDOUT_FILENO);
    saved_stderr = dup(STDERR_FILENO);

    if (saved_stdin < 0 || saved_stdout < 0 || saved_stderr < 0) {
        fprintf(stderr, "myshell: failed to save standard streams\n");
        if (saved_stdin >= 0) {
            close(saved_stdin);
        }
        if (saved_stdout >= 0) {
            close(saved_stdout);
        }
        if (saved_stderr >= 0) {
            close(saved_stderr);
        }
        return 1;
    }

    if (setup_redirection(args) != 0) {
        status = 1;
    } else {
        status = execute_builtin_command(args);
    }

    dup2(saved_stdin, STDIN_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);
    dup2(saved_stderr, STDERR_FILENO);

    close(saved_stdin);
    close(saved_stdout);
    close(saved_stderr);

    return status;
}

int execute_external(char **args)
{
    pid_t pid;
    int status;
    char command_text[MAX_LINE] = {0};
    char **expanded_args = expand_glob_patterns(args);

    if (expanded_args == NULL) {
        return 1;
    }

    for (int i = 0; expanded_args[i] != NULL; i++) {
        if (i > 0 && strlen(command_text) < MAX_LINE - 1) {
            strncat(command_text, " ", MAX_LINE - strlen(command_text) - 1);
        }
        strncat(command_text, expanded_args[i], MAX_LINE - strlen(command_text) - 1);
    }

    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "myshell: fork failed\n");
        free_args(expanded_args);
        return 1;
    }

    if (pid == 0) {
        setpgid(0, 0);
        restore_default_job_signals();

        if (setup_redirection(expanded_args) != 0) {
            exit(EXIT_FAILURE);
        }

        execvp(expanded_args[0], expanded_args);
        fprintf(stderr, "myshell: command not found: %s\n", expanded_args[0]);
        exit(EXIT_FAILURE);
    } else {
        setpgid(pid, pid);
        status = wait_for_foreground_job(pid, command_text);
        free_args(expanded_args);
    }

    return status;
}

/*
 * Make a trimmed heap copy so execute_line can keep splitting the same text.
 */
static char *trim_copy(const char *text)
{
    char *copy;
    size_t start = 0;
    size_t end;

    if (text == NULL) {
        return NULL;
    }

    while (text[start] == ' ' || text[start] == '\t' || text[start] == '\n') {
        start++;
    }

    end = strlen(text);
    while (end > start &&
           (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\n')) {
        end--;
    }

    copy = malloc(end - start + 1);
    if (copy == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return NULL;
    }

    memcpy(copy, text + start, end - start);
    copy[end - start] = '\0';
    return copy;
}

/*
 * Find a real background '&' while ignoring && and any ampersand inside quotes.
 */
static const char *find_background_operator(const char *line)
{
    int in_single_quote = 0;
    int in_double_quote = 0;

    for (const char *ptr = line; *ptr != '\0'; ptr++) {
        if (*ptr == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        }

        if (*ptr == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }

        if (!in_single_quote && !in_double_quote && *ptr == '&') {
            if ((ptr == line || *(ptr - 1) != '&') && *(ptr + 1) != '&') {
                return ptr;
            }
        }
    }

    return NULL;
}

/*
 * Run one command line through the existing Phase 1~3 execution path.
 */
static int execute_phase3_line(char *line, int *running)
{
    char **args;
    char *original_line;
    int status = 0;

    original_line = strdup(line);
    if (original_line == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return 1;
    }

    if (find_operator_outside_quotes(original_line, ";") != NULL) {
        char *line_copy = strdup(original_line);
        execute_chained_commands(line_copy, PIPE_SEQ);
        free(line_copy);
        free(original_line);
        return 0;
    }

    {
        PipeOperator op = detect_operator(original_line);

        if (op == PIPE_AND || op == PIPE_OR) {
            char *line_copy = strdup(original_line);
            execute_chained_commands(line_copy, op);
            free(line_copy);
            free(original_line);
            return 0;
        }

        if (op == PIPE_BASIC) {
            int num_commands = 0;
            char *line_copy = strdup(original_line);
            char **commands = split_by_pipe(line_copy, &num_commands);

            execute_pipeline(commands, num_commands);

            for (int i = 0; i < num_commands; i++) {
                free(commands[i]);
            }
            free(commands);
            free(line_copy);
            free(original_line);
            return 0;
        }
    }

    args = parse_line(line);
    if (args == NULL || args[0] == NULL) {
        free_args(args);
        free(original_line);
        return 1;
    }

    if (is_builtin(args[0])) {
        status = execute_builtin_with_redirection(args);
        if (strcmp(args[0], "exit") == 0 && status == 0 && running != NULL) {
            *running = 0;
        }
    }
    else {
        status = execute_external(args);
    }

    free_args(args);
    free(original_line);
    return status;
}

/*
 * Phase 4 entry point for one full input line, including background splits.
 */
int execute_line(char *line, int *running)
{
    char *working_line;
    int last_status = 0;

    if (line == NULL) {
        return 1;
    }

    working_line = trim_copy(line);
    if (working_line == NULL) {
        return 1;
    }

    while (working_line[0] != '\0') {
        const char *bg_ptr = find_background_operator(working_line);

        if (bg_ptr == NULL) {
            last_status = execute_phase3_line(working_line, running);
            break;
        }

        {
            size_t left_len = (size_t) (bg_ptr - working_line);
            char *left = malloc(left_len + 1);
            char *remainder;

            if (left == NULL) {
                fprintf(stderr, "Memory allocation error\n");
                free(working_line);
                return 1;
            }

            memcpy(left, working_line, left_len);
            left[left_len] = '\0';

            remainder = trim_copy(bg_ptr + 1);

            {
                char *trimmed_left = trim_copy(left);
                if (trimmed_left != NULL && trimmed_left[0] != '\0') {
                    /*
                     * Send the whole block to the background, not just one small
                     * command. For example, "echo a ; echo b &" runs that full
                     * sequential block in the background, which matches the tests
                     * better and keeps this path easier to extend later.
                     */
                    last_status = launch_background_job(trimmed_left);
                }
                free(trimmed_left);
            }

            free(left);
            free(working_line);

            if (remainder == NULL) {
                return last_status;
            }

            while (remainder[0] == ';' || remainder[0] == ' ') {
                memmove(remainder, remainder + 1, strlen(remainder));
            }

            if (strncmp(remainder, "&&", 2) == 0 || strncmp(remainder, "||", 2) == 0) {
                memmove(remainder, remainder + 2, strlen(remainder + 2) + 1);
            }

            working_line = trim_copy(remainder);
            free(remainder);

            if (working_line == NULL) {
                return 1;
            }
        }
    }

    free(working_line);
    return last_status;
}

