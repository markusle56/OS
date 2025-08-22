/*********************************************************************
   Program  : miniShell                   Version    : 1.3 (fixed)
 --------------------------------------------------------------------
   skeleton code for linux/unix/minix command line interpreter
 --------------------------------------------------------------------
   File             : minishell.c
   Compiler/System  : gcc/linux
********************************************************************/

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#define NV 20            /* max number of command tokens */
#define NL 100           /* input buffer size */
char line[NL];           /* command input buffer */
#define MAX_JOBS 10      /* max number of background jobs */

struct job {
    pid_t pid;
    char cmd[NL];
};

static struct job jobs[MAX_JOBS];
static int job_count = 0;

/* ---------------- Background job helpers ---------------- */

static int add_job(pid_t pid, const char *cmd) {
    if (job_count >= MAX_JOBS) return -1;
    jobs[job_count].pid = pid;
    strncpy(jobs[job_count].cmd, cmd, NL - 1);
    jobs[job_count].cmd[NL - 1] = '\0';
    job_count++;
    return job_count; /* job id is 1-based */
}

static int find_job_index_by_pid(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) return i;
    }
    return -1;
}

static void remove_job_by_index(int idx) {
    if (idx < 0 || idx >= job_count) return;
    for (int i = idx; i < job_count - 1; i++) {
        jobs[i] = jobs[i + 1];
    }
    job_count--;
}

/* Reap any finished background children and report "[id]+ Done  <cmd>" */
static void reap_background_finished(void) {
    int status;
    pid_t p;
    while ((p = waitpid(-1, &status, WNOHANG)) > 0) {
        int idx = find_job_index_by_pid(p);
        if (idx >= 0) {
            int job_id = idx + 1;
            printf("[%d]+ Done %s\n", job_id, jobs[idx].cmd);
            fflush(stdout);
            remove_job_by_index(idx);
        }
    }
    if (p == -1 && errno != ECHILD) {
        perror("waitpid");
    }
}

/* ---------------- Prompt ---------------- */

static void prompt(void) {
    // fputs("msh> ", stdout);
    fflush(stdout);
}

/* ---------------- Main ---------------- */

int main(int argk, char *argv[], char *envp[]) {
    (void)argk; (void)argv; (void)envp;

    /* Ignore SIGINT in the shell so Ctrl-C doesn’t kill the shell itself */
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        perror("signal(SIGINT, SIG_IGN)");
        return 1;
    }

    char *v[NV];
    const char *sep = " \t\n";

    while (1) {
        reap_background_finished();
        prompt();

        if (!fgets(line, NL, stdin)) { /* EOF or error */
            if (feof(stdin)) break;
            if (ferror(stdin)) { clearerr(stdin); continue; }
        }

        reap_background_finished();

        if (feof(stdin)) break;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') continue;

        /* Tokenize */
        int i = 0;
        v[i] = strtok(line, sep);
        while (v[i] && i + 1 < NV) {
            i++;
            v[i] = strtok(NULL, sep);
        }
        /* Ensure NULL-terminated argv for execvp */
        if (i == NV) i = NV - 1;
        v[i] = NULL;
        int argc = i; /* number of tokens excluding final NULL (approx) */
        if (!v[0]) continue;

        /* Built-ins */
        if (strcmp(v[0], "exit") == 0 || strcmp(v[0], "quit") == 0) {
            break;
        }

        if (strcmp(v[0], "cd") == 0) {
            const char *dest = NULL;
            if (argc == 0 || v[1] == NULL) {
                dest = getenv("HOME");
                if (!dest) dest = "/";
            } else {
                dest = v[1];
            }
            if (chdir(dest) == -1) {
                perror("chdir");
            }
            continue;
        }

        /* Background? */
        int background = 0;
        if (i > 0 && v[i - 1] && strcmp(v[i - 1], "&") == 0) {
            background = 1;
            v[i - 1] = NULL; /* remove '&' */
            argc--;
        }

        /* Reconstruct command line for job list */
        char cmdline_for_job[NL] = {0};
        {
            size_t pos = 0;
            for (int k = 0; v[k]; k++) {
                size_t len = strlen(v[k]);
                if (pos + len + 1 >= sizeof(cmdline_for_job)) break;
                memcpy(cmdline_for_job + pos, v[k], len);
                pos += len;
                if (v[k + 1] && pos + 1 < sizeof(cmdline_for_job)) {
                    cmdline_for_job[pos++] = ' ';
                }
            }
            if (pos < sizeof(cmdline_for_job)) cmdline_for_job[pos] = '\0';
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        } else if (pid == 0) {
            /* Child: restore default SIGINT so Ctrl-C can interrupt it */
            signal(SIGINT, SIG_DFL);
            execvp(v[0], v);
            perror("execvp");
            _exit(127);
        } else {
            if (background) {
                int job_id = add_job(pid, cmdline_for_job);
                if (job_id == -1) {
                    fprintf(stderr, "Too many background jobs; not tracking PID %d\n", pid);
                }
                /* Do not wait; job will be reported when finished */
            } else {
                /* Foreground: wait for child */
                int status;
                pid_t w;
                do {
                    w = waitpid(pid, &status, 0);
                } while (w == -1 && errno == EINTR);
                if (w == -1) perror("waitpid");
            }
        }
    }

    /* On exit, reap any remaining children */
    while (waitpid(-1, NULL, WNOHANG) > 0) { /* no-op */ }

    return 0;
}
