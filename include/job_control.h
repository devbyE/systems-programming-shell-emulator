/*
 * File: job_control.h
 * Description:
 * Declarations for the Phase 4 job-control layer.
 * This is the main entry point for background jobs, foreground tracking,
 * and built-ins like jobs, fg, bg, and wait.
 */

#ifndef JOB_CONTROL_H
#define JOB_CONTROL_H

#include <sys/types.h>
#include "shell.h"

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} JobStatus;

typedef struct {
    int active;
    int id;
    pid_t pid;
    JobStatus status;
    int exit_status;
    char command[MAX_LINE];
} JobEntry;

void init_job_control(void);
void poll_job_notifications(void);
void set_foreground_pgid(pid_t pgid);
void clear_foreground_pgid(void);
void restore_default_job_signals(void);
void print_finished_jobs(void);

int launch_background_job(const char *command);
int wait_for_foreground_job(pid_t pid, const char *command);

int jobs_builtin(char **args);
int fg_builtin(char **args);
int bg_builtin(char **args);
int wait_builtin(char **args);

int register_stopped_foreground_job(pid_t pid, const char *command, int status);

#endif
