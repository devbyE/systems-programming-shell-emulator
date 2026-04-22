/*
 * File: job_control.c
 * Description:
 * Phase 4 job control support.
 *
 * Jobs are tracked here as whole command lines instead of individual commands.
 * That keeps the Phase 4 layer on top of the Phase 3 pipe / redirection /
 * && / || / ; flow without tearing up code that already works.
 *
 * The current layout is intentionally simple:
 * 1. Foreground commands keep using the existing Phase 1~3 execution flow.
 * 2. The shell only forks a subshell when it sees a background '&'.
 * 3. That child runs the full command line on its own.
 * 4. jobs / fg / bg / wait all manage that subshell as one job.
 *
 * If we want to move closer to bash-style job control later, this version
 * should still make the current boundaries easy to follow.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../include/shell.h"
#include "../include/job_control.h"

static JobEntry job_table[MAX_JOBS];
static int next_job_id = 1;
static volatile sig_atomic_t current_foreground_pgid = 0;
static volatile sig_atomic_t sigchld_pending = 0;

/*
 * Convert the internal enum into the text that jobs prints.
 */
static const char *job_status_string(JobStatus status)
{
    if (status == JOB_RUNNING) {
        return "Running";
    }
    if (status == JOB_STOPPED) {
        return "Stopped";
    }
    return "Done";
}

/*
 * Trim a stored command string in place before we keep it in the job table.
 */
static void trim_whitespace_inplace(char *text)
{
    size_t start = 0;
    size_t end;

    if (text == NULL) {
        return;
    }

    while (text[start] == ' ' || text[start] == '\t' || text[start] == '\n') {
        start++;
    }

    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }

    end = strlen(text);
    while (end > 0 &&
           (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\n')) {
        text[end - 1] = '\0';
        end--;
    }
}

/*
 * Find the next free slot in the fixed-size job table.
 */
static JobEntry *allocate_job_slot(void)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!job_table[i].active) {
            memset(&job_table[i], 0, sizeof(job_table[i]));
            job_table[i].active = 1;
            job_table[i].id = next_job_id++;
            return &job_table[i];
        }
    }

    return NULL;
}

/*
 * Look up a job by the process id we are tracking for that job.
 */
static JobEntry *find_job_by_pid(pid_t pid)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].active && job_table[i].pid == pid) {
            return &job_table[i];
        }
    }

    return NULL;
}

/*
 * Look up a job by its shell-visible job number.
 */
static JobEntry *find_job_by_id(int id)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].active && job_table[i].id == id) {
            return &job_table[i];
        }
    }

    return NULL;
}

/*
 * Return the most recent active job, which we treat as the current job.
 */
static JobEntry *find_current_job(void)
{
    JobEntry *candidate = NULL;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].active) {
            if (candidate == NULL || job_table[i].id > candidate->id) {
                candidate = &job_table[i];
            }
        }
    }

    return candidate;
}

/*
 * Return the job just before the current one, for %- style lookups.
 */
static JobEntry *find_previous_job(void)
{
    JobEntry *current = NULL;
    JobEntry *previous = NULL;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (!job_table[i].active) {
            continue;
        }

        if (current == NULL || job_table[i].id > current->id) {
            previous = current;
            current = &job_table[i];
        }
        else if (previous == NULL || job_table[i].id > previous->id) {
            previous = &job_table[i];
        }
    }

    return previous;
}

/*
 * Clear a slot once the shell no longer needs to track that job.
 */
static void remove_job(JobEntry *job)
{
    if (job == NULL) {
        return;
    }

    job->active = 0;
    job->id = 0;
    job->pid = 0;
    job->status = JOB_DONE;
    job->exit_status = 0;
    job->command[0] = '\0';
}

/*
 * Add a new job entry after a background launch or a foreground stop.
 */
static JobEntry *add_job(pid_t pid, const char *command, JobStatus status)
{
    JobEntry *job = allocate_job_slot();

    if (job == NULL) {
        fprintf(stderr, "myshell: job table is full\n");
        return NULL;
    }

    job->pid = pid;
    job->status = status;
    job->exit_status = 0;
    strncpy(job->command, command, MAX_LINE - 1);
    job->command[MAX_LINE - 1] = '\0';
    trim_whitespace_inplace(job->command);
    return job;
}

/*
 * Parse job specs like %1, 1, %+, and %- into a concrete table entry.
 */
static JobEntry *resolve_job_spec(const char *spec)
{
    int id = 0;
    const char *text = spec;

    if (spec == NULL) {
        return find_current_job();
    }

    if (strcmp(spec, "%+") == 0 || strcmp(spec, "+") == 0) {
        return find_current_job();
    }

    if (strcmp(spec, "%-") == 0 || strcmp(spec, "-") == 0) {
        return find_previous_job();
    }

    if (spec[0] == '%') {
        text = spec + 1;
    }

    if (*text == '\0') {
        return find_current_job();
    }

    for (const char *ptr = text; *ptr != '\0'; ptr++) {
        if (*ptr < '0' || *ptr > '9') {
            return NULL;
        }
        id = (id * 10) + (*ptr - '0');
    }

    if (id <= 0) {
        return NULL;
    }

    return find_job_by_id(id);
}

/*
 * Consume pending child state changes and mirror them into the job table.
 */
static void reap_children(void)
{
    int status = 0;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        JobEntry *job = find_job_by_pid(pid);

        if (job == NULL) {
            continue;
        }

        if (WIFSTOPPED(status)) {
            job->status = JOB_STOPPED;
        }
        else if (WIFCONTINUED(status)) {
            job->status = JOB_RUNNING;
        }
        else {
            job->status = JOB_DONE;
            if (WIFEXITED(status)) {
                job->exit_status = WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status)) {
                job->exit_status = 128 + WTERMSIG(status);
            }
        }
    }

    sigchld_pending = 0;
}

/*
 * Keep the SIGCHLD handler tiny and do the real work later in the main flow.
 */
static void handle_sigchld(int signo)
{
    (void) signo;
    sigchld_pending = 1;
}

/*
 * Forward interactive signals to the current foreground process group.
 */
static void forward_signal_to_foreground(int signo)
{
    pid_t pgid = (pid_t) current_foreground_pgid;

    if (pgid > 0) {
        kill(-pgid, signo);
    }
}

/*
 * Ctrl+C should affect the foreground job, not the shell itself.
 */
static void handle_sigint(int signo)
{
    (void) signo;
    forward_signal_to_foreground(SIGINT);
}

/*
 * Ctrl+Z should stop the foreground job as a group.
 */
static void handle_sigtstp(int signo)
{
    (void) signo;
    forward_signal_to_foreground(SIGTSTP);
}

/*
 * Small helper to install one signal handler with the flags we want here.
 */
static int install_signal_handler(int signo, void (*handler)(int))
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (signo == SIGCHLD) {
        sa.sa_flags |= SA_NOCLDSTOP;
    }

    if (sigaction(signo, &sa, NULL) != 0) {
        perror("sigaction");
        return -1;
    }

    return 0;
}

/*
 * Set up the signal side of Phase 4 before the shell loop starts running.
 */
void init_job_control(void)
{
    install_signal_handler(SIGCHLD, handle_sigchld);
    install_signal_handler(SIGINT, handle_sigint);
    install_signal_handler(SIGTSTP, handle_sigtstp);
}

/*
 * Process deferred SIGCHLD work at safe points in the main thread.
 */
void poll_job_notifications(void)
{
    if (sigchld_pending) {
        reap_children();
    }
}

/*
 * Remove jobs that are already marked done so jobs only shows live entries.
 */
void print_finished_jobs(void)
{
    poll_job_notifications();

    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].active && job_table[i].status == JOB_DONE) {
            remove_job(&job_table[i]);
        }
    }
}

/*
 * Remember which process group currently owns the foreground.
 */
void set_foreground_pgid(pid_t pgid)
{
    current_foreground_pgid = (sig_atomic_t) pgid;
}

/*
 * Clear the foreground owner after a foreground wait finishes.
 */
void clear_foreground_pgid(void)
{
    current_foreground_pgid = 0;
}

/*
 * Child jobs should not inherit the shell's custom signal handlers.
 */
void restore_default_job_signals(void)
{
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
}

/*
 * Start one background job as a subshell that runs a full command line.
 */
int launch_background_job(const char *command)
{
    pid_t pid = fork();
    JobEntry *job;

    if (pid < 0) {
        fprintf(stderr, "myshell: fork failed\n");
        return 1;
    }

    if (pid == 0) {
        int child_running = 1;
        char *line_copy;

        setpgid(0, 0);
        restore_default_job_signals();

        line_copy = strdup(command);
        if (line_copy == NULL) {
            fprintf(stderr, "myshell: memory allocation error\n");
            exit(1);
        }

        /*
         * Reuse execute_line here so background and foreground commands keep
         * sharing the same parsing path. That way the Phase 3 fixes for pipes,
         * quotes, and redirection stay in one place.
         */
        exit(execute_line(line_copy, &child_running));
    }

    setpgid(pid, pid);
    job = add_job(pid, command, JOB_RUNNING);
    if (job == NULL) {
        kill(pid, SIGTERM);
        return 1;
    }

    printf("[%d] %d\n", job->id, job->pid);
    fflush(stdout);
    return 0;
}

/*
 * If a foreground job stops, move it into the table so fg/bg/jobs can find it.
 */
int register_stopped_foreground_job(pid_t pid, const char *command, int status)
{
    JobEntry *job = find_job_by_pid(pid);

    if (job == NULL) {
        job = add_job(pid, command, JOB_STOPPED);
        if (job == NULL) {
            return 1;
        }
    }

    job->status = JOB_STOPPED;
    job->exit_status = status;
    return 0;
}

/*
 * Wait for a foreground job and convert the result into a shell-style status.
 */
int wait_for_foreground_job(pid_t pid, const char *command)
{
    int status = 0;

    set_foreground_pgid(pid);

    while (1) {
        pid_t result = waitpid(pid, &status, WUNTRACED);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            clear_foreground_pgid();
            fprintf(stderr, "myshell: waitpid failed\n");
            return 1;
        }

        break;
    }

    clear_foreground_pgid();

    if (WIFSTOPPED(status)) {
        register_stopped_foreground_job(pid, command, status);
        return 128 + WSTOPSIG(status);
    }

    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return 1;
}

/*
 * Implement jobs, with optional filtering for running jobs only.
 */
int jobs_builtin(char **args)
{
    int running_only = 0;
    int printed = 0;

    poll_job_notifications();

    if (args[1] != NULL && strcmp(args[1], "-r") == 0) {
        running_only = 1;
    }

    for (int i = 0; i < MAX_JOBS; i++) {
        if (!job_table[i].active) {
            continue;
        }

        if (running_only && job_table[i].status != JOB_RUNNING) {
            continue;
        }

        if (job_table[i].status == JOB_DONE) {
            continue;
        }

        printf("[%d] %s %d %s\n",
               job_table[i].id,
               job_status_string(job_table[i].status),
               job_table[i].pid,
               job_table[i].command);
        fflush(stdout);
        printed = 1;
    }

    if (!printed) {
        printf("no jobs\n");
        fflush(stdout);
    }

    return 0;
}

/*
 * Bring one tracked job back to the foreground and wait on it there.
 */
int fg_builtin(char **args)
{
    JobEntry *job = resolve_job_spec(args[1]);
    int status;

    poll_job_notifications();

    if (job == NULL || !job->active || job->status == JOB_DONE) {
        fprintf(stderr, "myshell: fg: no such job\n");
        return 1;
    }

    /*
     * fg is kept pretty direct for now: resume first, then wait on it as the
     * foreground job. Since one job maps to one subshell at the moment,
     * signaling the whole process group keeps this path simple.
     */
    if (kill(-job->pid, SIGCONT) != 0 && kill(job->pid, SIGCONT) != 0) {
        perror("myshell: fg");
        return 1;
    }

    job->status = JOB_RUNNING;
    status = wait_for_foreground_job(job->pid, job->command);

    if (job->active && job->status != JOB_STOPPED) {
        remove_job(job);
    }

    return status;
}

/*
 * Resume a stopped job in the background.
 */
int bg_builtin(char **args)
{
    JobEntry *job = resolve_job_spec(args[1]);

    poll_job_notifications();

    if (args[1] == NULL) {
        fprintf(stderr, "myshell: bg: job id required\n");
        return 1;
    }

    if (job == NULL || !job->active || job->status == JOB_DONE) {
        fprintf(stderr, "myshell: bg: no such job\n");
        return 1;
    }

    if (kill(-job->pid, SIGCONT) != 0 && kill(job->pid, SIGCONT) != 0) {
        perror("myshell: bg");
        return 1;
    }

    job->status = JOB_RUNNING;
    printf("[%d] %d %s\n", job->id, job->pid, job->command);
    fflush(stdout);
    return 0;
}

/*
 * Wait for one specific job or for all tracked jobs when no id is given.
 */
int wait_builtin(char **args)
{
    JobEntry *job;
    int status = 0;

    poll_job_notifications();

    if (args[1] == NULL) {
        for (int i = 0; i < MAX_JOBS; i++) {
            if (!job_table[i].active || job_table[i].status == JOB_DONE) {
                continue;
            }

            if (waitpid(job_table[i].pid, &status, 0) >= 0) {
                remove_job(&job_table[i]);
            }
        }

        return 0;
    }

    job = resolve_job_spec(args[1]);
    if (job == NULL || !job->active || job->status == JOB_DONE) {
        fprintf(stderr, "myshell: wait: no such job\n");
        return 1;
    }

    if (waitpid(job->pid, &status, 0) < 0) {
        perror("myshell: wait");
        return 1;
    }

    remove_job(job);
    return 0;
}
