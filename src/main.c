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
#include "../include/shell.h"
#include "../include/job_control.h"

int main(void)
{
    char *line;
    int running = 1;

    init_job_control();

    while (running) {
        poll_job_notifications();
        print_finished_jobs();
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

        execute_line(line, &running);
        free(line);
    }

    return 0;
}
