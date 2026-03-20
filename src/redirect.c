/*
 * File: redirect.c
 * Author: Efrem Wilkerson
 * Description:
 * Implements input, output, append, and error redirection.
 * This file scans the argument list for <, >, >>, and 2>
 * and uses open() and dup2() to redirect file descriptors
 * before execvp runs in the child process.
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../include/shell.h"
#include "../include/redirect.h"

static int ensure_parent_directories(const char *path)
{
    char *path_copy;
    char *separator;
    char *current;

    if (path == NULL || strchr(path, '/') == NULL) {
        return 0;
    }

    path_copy = malloc(strlen(path) + 1);
    if (path_copy == NULL) {
        fprintf(stderr, "Memory allocation error\n");
        return -1;
    }

    strcpy(path_copy, path);
    separator = strrchr(path_copy, '/');
    if (separator == NULL) {
        free(path_copy);
        return 0;
    }

    *separator = '\0';
    if (path_copy[0] == '\0') {
        free(path_copy);
        return 0;
    }

    current = path_copy;
    if (current[0] == '/') {
        current++;
    }

    while (*current != '\0') {
        if (*current == '/') {
            *current = '\0';
            if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
                perror("myshell: mkdir");
                free(path_copy);
                return -1;
            }
            *current = '/';
        }
        current++;
    }

    if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
        perror("myshell: mkdir");
        free(path_copy);
        return -1;
    }

    free(path_copy);
    return 0;
}

static int open_output_target(const char *path, int flags)
{
    if (ensure_parent_directories(path) != 0) {
        return -1;
    }

    return open(path, flags, 0644);
}

/*
 * Function: setup_redirection
 * Description:
 * Checks the argument list for redirection operators:
 *   <   input redirection
 *   >   output redirection (overwrite)
 *   >>  output redirection (append)
 *   2>  error redirection
 
 * Opens the requested files and redirects stdin, stdout,
 * or stderr using dup2() before the command is executed.
 
 * Redirection tokens and filenames are removed from the
 * argument list so execvp() only receives the real command.
 * Parameters:
 *   args - argument list passed to execvp
 * Returns:
 *   0 on success
 *  -1 on failure
 */

int setup_redirection(char **args)
{
    int i = 0;
    int j = 0;
    int fd;
    char *clean_args[MAX_ARGS];

    while (args[i] != NULL) {
        if (strcmp(args[i], "<") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "myshell: missing input file after <\n");
                return -1;
            }

            fd = open(args[i + 1], O_RDONLY);
            if (fd < 0) {
                perror("myshell: input redirection");
                return -1;
            }

// dup2 replaces standard input (fd 0) with the opened file descriptor            
            if (dup2(fd, STDIN_FILENO) < 0) {
                perror("myshell: dup2 input");
                close(fd);
                return -1;
            }

            close(fd);
            free(args[i]);
            free(args[i + 1]);
            i += 2;
        }
        else if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "myshell: missing output file after >\n");
                return -1;
            }

            fd = open_output_target(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC);
            if (fd < 0) {
                perror("myshell: output redirection");
                return -1;
            }
// dup2 replaces standard output (fd 1) with the opened file descriptor
            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("myshell: dup2 output");
                close(fd);
                return -1;
            }

            close(fd);
            free(args[i]);
            free(args[i + 1]);
            i += 2;
        }
        else if (strcmp(args[i], ">>") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "myshell: missing output file after >>\n");
                return -1;
            }

            fd = open_output_target(args[i + 1], O_WRONLY | O_CREAT | O_APPEND);
            if (fd < 0) {
                perror("myshell: append redirection");
                return -1;
            }

            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("myshell: dup2 append");
                close(fd);
                return -1;
            }

            close(fd);
            free(args[i]);
            free(args[i + 1]);
            i += 2;
        }
        else if (strcmp(args[i], "2>") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "myshell: missing error file after 2>\n");
                return -1;
            }

            fd = open_output_target(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC);
            if (fd < 0) {
                perror("myshell: error redirection");
                return -1;
            }
// dup2 replaces standard error (fd 2) with the opened file descriptor
            if (dup2(fd, STDERR_FILENO) < 0) {
                perror("myshell: dup2 stderr");
                close(fd);
                return -1;
            }

            close(fd);
            free(args[i]);
            free(args[i + 1]);
            i += 2;
        }
        else {
            clean_args[j] = args[i];
            j++;
            i++;
        }
    }

    clean_args[j] = NULL;

    i = 0;
    while (clean_args[i] != NULL) {
        args[i] = clean_args[i];
        i++;
    }
    args[i] = NULL;

    return 0;
}
