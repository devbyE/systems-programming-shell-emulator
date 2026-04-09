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
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "../include/shell.h"
#include "../include/redirect.h"

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
            i += 2;
        }
        else if (strcmp(args[i], ">") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "myshell: missing output file after >\n");
                return -1;
            }

            fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
            i += 2;
        }
        else if (strcmp(args[i], ">>") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "myshell: missing output file after >>\n");
                return -1;
            }

            fd = open(args[i + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
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
            i += 2;
        }
        else if (strcmp(args[i], "2>") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "myshell: missing error file after 2>\n");
                return -1;
            }

            fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
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