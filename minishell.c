/*********************************************************************
   Program  : miniShell                   Version    : 1.3
 --------------------------------------------------------------------
   skeleton code for linix/unix/minix command line interpreter
 --------------------------------------------------------------------
   File			: minishell.c
   Compiler/System	: gcc/linux

********************************************************************/

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#define NV 20			/* max number of command tokens */
#define NL 100			/* input buffer size */
char    line[NL];	    /* command input buffer */
#define MAX_JOBS 10     /* max number of background jobs*/

// Define job struct to s   tore background job
struct job {
    int   job_id;
    pid_t pid;
    char cmd[NL];
};

// Initialise jobs and job_count to track the background jobs
struct job jobs[MAX_JOBS];
int job_count = 0;
static int next_job_id = 1;


// Helper function to handle background job

// Add job into the jobs list
int add_job(pid_t pid, const char *cmd) {
    if (job_count >= MAX_JOBS) return -1;
    jobs[job_count].pid = pid;
    jobs[job_count].job_id = next_job_id++;
    strncpy(jobs[job_count].cmd, cmd, NL - 1);
    jobs[job_count].cmd[NL-1] = '\0';
    job_count++;
    return jobs[job_count - 1].job_id; 
}

// Find the job index by the given pid in the jobs list 
int find_job_index_by_pid(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) return i;
    };
    return -1;
}

// Remove the job by the given index
void remove_job_by_index(int idx) {
    if (idx < 0 || idx >= job_count) return;
    for (int i = idx; i < job_count - 1; i++) {  // Shift the all the remain jobs to the left of the list 
        jobs[i] = jobs[i+1];
    };
    job_count--;
}

// Reap any finished background children and report "[id]+ Done  <cmd>"
void reap_background_finished(void) {
    int status;
    pid_t p;
    // waitpid(-1, &status, WNOHANG) to wait for any child process to end, return its pid and store status value in status variable
    while ((p = waitpid(-1, &status, WNOHANG)) > 0) {
        int idx = find_job_index_by_pid(p);
        if (idx >= 0) {
            printf("[%d]+ Done                 %s\n", jobs[idx].job_id, jobs[idx].cmd);
            fflush(stdout);
            remove_job_by_index(idx);
        }
    }
    if (p == -1 && errno != ECHILD) {
        perror("waitpid"); 
    }
}

/*
	shell prompt
 */

void prompt(void)
{
  // ## REMOVE THIS 'fprintf' STATEMENT BEFORE SUBMISSION
//   fprintf(stdout, "\n msh> ");
  fflush(stdout);
}


/* argk - number of arguments */
/* argv - argument vector from command line */
/* envp - environment pointer */
int main(int argk, char *argv[], char *envp[])
{
    (void)argk; (void)argv; (void)envp;
    int             frkRtnVal;	        /* value returned by fork sys call */
    char           *v[NV];	            /* array of pointers to command line tokens */
    char           *sep = " \t\n";      /* command line token separators    */
    int             i;		            /* parse index */

    /* prompt for and process one command line at a time  */

    while (1) {			/* do Forever */
        reap_background_finished();
        prompt();
        fgets(line, NL, stdin);
        // fflush(stdin);
        reap_background_finished();

        // This if() required for gradescope
        if (feof(stdin)) {        /* EOF: finish & report bg jobs */
           while (job_count > 0) {
            int status;
            pid_t p = waitpid(-1, &status, 0);
            if (p == -1) {
                    if (errno == ECHILD) break;
                    perror("waitpid");
                    break;
                }
                int idx = find_job_index_by_pid(p);
                if (idx >= 0) {
                    printf("[%d]+ Done                 %s\n", jobs[idx].job_id, jobs[idx].cmd); 
                    fflush(stdout);
                    remove_job_by_index(idx);
                }
            }   
            exit(0);
        }
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\000') {
            continue;			/* to prompt */
        }

        v[0] = strtok(line, sep);
        for (i = 1; i < NV; i++) {
            v[i] = strtok(NULL, sep);
            if (v[i] == NULL){
                break;
            }
        }
        int argc = i;   /* number of tokens */
        if (v[0] == NULL) continue; /* empty line safety */ 

        if (strcmp(v[0], "cd") == 0) {
            const char *dest = NULL;

            if (argc == 1) {
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

        if (strcmp(v[0], "exit") == 0 || strcmp(v[0], "quit") == 0) {
            reap_background_finished();
            return 0;
        }

        int background = 0;
        if (argc > 0 && strcmp(v[argc - 1], "&") == 0) {
            background = 1;
            v[argc - 1] = NULL;  /* remove '&' from argv */
            argc--;              /* optional: keep argc consistent */
        }

        char cmdline_for_job[NL] = {0};
        {
            size_t pos = 0;
            for (int k = 0; k < argc && v[k]; k++) {
                size_t len = strlen(v[k]);
                if (pos + len + 2 >= sizeof(cmdline_for_job)) break;
                memcpy(cmdline_for_job + pos, v[k], len);
                pos += len;
                if (k+1 < argc && v[k+1]) {
                    cmdline_for_job[pos++] = ' ';
                } else {
                    cmdline_for_job[pos] = '\0';
                }
            }
        }

        /* fork a child process to exec the command in v[0] */
        switch (frkRtnVal = fork()) {
        case -1:			/* fork returns error to parent process */
        {
            perror("fork");  
            break;
        }
        case 0:			/* code executed only by child process */
        {
            execvp(v[0], v);
            perror("execvp");     // only reached if exec fails
            _exit(127);     
        }
        default:			/* code executed only by parent process */
        {
            if (background) {
                int job_id = add_job(frkRtnVal, cmdline_for_job);
                if (job_id >= 0) {
                    printf("[%d] %d\n", job_id, frkRtnVal);
                    fflush(stdout);
                }       
            } else {
                /* foreground: wait until child finishes */
                int status;
                pid_t wpid;
                do {
                    wpid = waitpid(frkRtnVal, &status, 0);
                } while (wpid == -1 && errno == EINTR);
                if (wpid == -1) {
                    perror("waitpid");
                }
            }
        }
        }				/* switch */
    }				/* while */
}				/* main */
