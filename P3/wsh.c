/**
 * Replicate a simplified version of a Unix Command Line Interpreter/Shell
 *
 * @author Mrigank Kumar
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "wsh.h"

static Job* all_jobs[128];

/////////////////////////// INITIALIZATION FUNCTIONS ///////////////////////////

Command* command_init(char* cmd) {
    if (!cmd)
        return NULL;
    ssize_t n = strlen(cmd);

    if (n <= 0)
        return NULL;

    Command* command = malloc(sizeof(Command));
    _MALLOC_CHECK_(command)

    command->argc = 0;

    int argv_size = 5;
    int argv_incr = 5;

    command->argv = malloc(sizeof(char*) * argv_size);
    _MALLOC_CHECK_(command->argv)

    char* end_ptr;
    char* token = strtok_r(cmd, _DELIMITER_, &end_ptr);

    while (NULL != token) {
        if (command->argc == argv_size) {
            argv_size += argv_incr;
            command->argv = realloc(command->argv, sizeof(char*) * (argv_incr));
            _MALLOC_CHECK_(command->argv)
        }

        n = strlen(token);

        command->argv[command->argc] = malloc(sizeof(char) * (n + 1));
        _MALLOC_CHECK_(command->argv[command->argc])

        strcpy(command->argv[command->argc++], token);

        token = strtok_r(NULL, _DELIMITER_, &end_ptr);
    }

    command->argv = realloc(command->argv, sizeof(char*) * (command->argc + 1));
    _MALLOC_CHECK_(command->argv)

    command->argv[command->argc] = NULL;

    return command;
}

Process* process_init(Command* cmd) {
    if (NULL == cmd) return NULL;

    Process* proc = malloc(sizeof(Process));
    _MALLOC_CHECK_(proc)

    proc->cmd = cmd;

    return proc;
}

Job* job_init(char* cmd, Process** procs, int n_procs, bool bg) {
    Job* job = malloc(sizeof(Job));
    _MALLOC_CHECK_(job)

    job->cmd = malloc(sizeof(char) * (strlen(cmd) + 1));
    _MALLOC_CHECK_(job->cmd);
    strcpy(job->cmd, cmd);

    job->processes = malloc(sizeof(Process*) * n_procs);
    _MALLOC_CHECK_(job->processes);

    for (int i = 0; i < n_procs; ++i) {
        job->processes[i] = procs[i];
    }

    job->n_process = n_procs;
    job->bg = bg;

    for (int i = JOB_START_IDX; i < MAX_JOBS; i++) {
        if (NULL == all_jobs[i]) {
            all_jobs[i] = job;
            break;
        }
    }

    job->p_state = FOREGROUND;

    return job;
}


///////////////////////// END INITIALIZATION FUNCTIONS /////////////////////////


///////////////////////////// DESTRUCTOR FUNCTIONS /////////////////////////////

void command_destroy(Command* cmd) {
    if (NULL == cmd) return;


    if (cmd->argv) {
        for (int i = 0; i < cmd->argc; i++) {
            free(cmd->argv[i]);
        }

        free(cmd->argv);
    }

    free(cmd);
}

void process_destroy(Process* process) {
    if (NULL == process) return;

    command_destroy(process->cmd);

    free(process);
}

void job_destroy(Job* job) {
    if (NULL == job) return;

    if (job->cmd)
        free(job->cmd);

    if (job->processes) {
        for (int i = 0; i < job->n_process; i++) {
            process_destroy(job->processes[i]);
        }

        free(job->processes);
    }

    for (int i = JOB_START_IDX; i < MAX_JOBS; i++) {
        if (all_jobs[i] == job) {
            all_jobs[i] = NULL;
            break;
        }
    }

    free(job);
}

/////////////////////////// END DESTRUCTOR FUNCTIONS ///////////////////////////


/////////////////////////// SIGNAL HANDLER FUNCTIONS ///////////////////////////

void sigchld_handler(int signal) {
    pid_t cpid = wait(NULL);

    if (cpid < 0) return;

    printf("\nDone %i\n", cpid);

    for (int i = 1; i < MAX_JOBS; i++) {
        Job* job = all_jobs[i];

        if (NULL == job) continue;

        if (job->processes[job->n_process - 1]->pid == cpid) {
            job_destroy(job);
            return;
        }
    }
}

void sigtstp_handler(int signal) {

}


///////////////////////// END SIGNAL HANDLER FUNCTIONS /////////////////////////


///////////////////////////// MAIN LOOP FUNCTIONS //////////////////////////////

void display_prompt() {
    printf(_PROMPT_);
    fflush(stdout);
}

char* get_command() {
    char* cmd = NULL;
    size_t size = 0;

    ssize_t n_chars = getline(&cmd, &size, stdin);

    if (n_chars < 0) {
        free(cmd);
        _FAILURE_EXIT_("getline failed to read input!\n")
    }

    return cmd;
}

Job* parse_command(char* input) {
    bool bg = false;
    ssize_t len = strlen(input);

    if (len <= 0) {
        return NULL;
    }

    char* cmd_cpy = malloc(sizeof(char) * (len + 1));
    strcpy(cmd_cpy, input);

    if (input[len - 1] == _AMPERSAND_) {
        bg = true;
        input[len - 1] = _NULL_TERMINATOR_;
    }

    Process* procs[32];
    int n_procs = 0;

    if (NULL != strstr(input, _PIPE_)) {
        char* end_str;
        char* token = strtok_r(input, _PIPE_, &end_str);

        while(NULL != token) {
            Command* cmd = command_init(token);
            if (NULL == cmd) _FAILURE_EXIT_("Command init failed!\n")

            procs[n_procs] = process_init(cmd);

            if (NULL == procs[n_procs]) _FAILURE_EXIT_("Process init failed!\n")

            n_procs++;

            token = strtok_r(NULL, _PIPE_, &end_str);
        }
    } else {
        Command* cmd = command_init(input);
        if (NULL == cmd) _FAILURE_EXIT_("Command init failed!\n")

        procs[n_procs] = process_init(cmd);

        if (NULL == procs[n_procs]) _FAILURE_EXIT_("Process init failed!\n")

        n_procs++;
    }

    Job* job = job_init(cmd_cpy, procs, n_procs, bg);

    free(cmd_cpy);

    if (NULL == job) _FAILURE_EXIT_("Job init failed!\n")

    return job;
}

void dispatch_job(Job* job) {

    pid_t cpid = fork();

    if (cpid < 0) _FAILURE_EXIT_("fork failed!\n")

    char** argv = job->processes[0]->cmd->argv;
    switch (cpid) {
        case 0: // child
            if (job->bg == true) {
                close(STDIN_FILENO);
            }
            execvp(argv[0], argv);

            _FAILURE_EXIT_("execvp failed!\n")

            break;
        default: // parent
            job->processes[0]->pid = cpid;
            if (job->bg == false) {
                waitpid(cpid, NULL, 0);
                job_destroy(job);
            } else {
                job->p_state = BACKGROUND;
            }
            break;
    }
}

void dispatch_piped_jobs(Job* job) {
    int n_pipes = job->n_process - 1;

    int pipes[n_pipes][2];

    for (int i = 0; i < n_pipes; i++) {
        if (pipe(pipes[i]) < 0) _FAILURE_EXIT_("pipe failed!\n")
    }

    pid_t cpid;
    for (int i = 0; i < job->n_process; i++) {
        cpid = fork();

        if (cpid < 0) _FAILURE_EXIT_("Fork failure\n")

        if (0 == cpid) {
            // close write ends of pipes for previous processes
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }

            // close read ends of pipes for following processes
            if (i < n_pipes) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Close all remaining pipe file descriptors
            for (int j = 0; j < n_pipes; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            char** argv = job->processes[i]->cmd->argv;

            execvp(argv[0], argv);

            _FAILURE_EXIT_("execvp failed!\n");
        } else {
            job->processes[i]->pid = cpid;

            // Close all remaining pipe file descriptors in parent as well
            if (i > 0) {
                close(pipes[i-1][0]);
            }
            if (i < n_pipes) {
                close(pipes[i][1]);
            }
        }
    }

    if (job->bg == false) {
        for (int i = 0; i < job->n_process; i++) {
            waitpid(job->processes[i]->pid, NULL, 0);
        }
        job_destroy(job);
    } else {
        job->p_state = BACKGROUND;
    }
}


/////////////////////////// END MAIN LOOP FUNCTIONS ////////////////////////////


///////////////////////// BUILT-IN COMMANDS FUNCTIONS //////////////////////////

int check_builtin(Job* job) {
    if (job->n_process != 1) {
        return 0;
    }

    char* command = job->processes[0]->cmd->argv[0];

    if (strncmp(command, _BUITLINS_BG_, strlen(_BUITLINS_BG_)) == 0) {
        builtins_bg(job->processes[0]->cmd);
    }
    else if (strncmp(command, _BUITLINS_CD_, strlen(_BUITLINS_CD_)) == 0) {
        builtins_cd(job->processes[0]->cmd);
    }
    else if (strncmp(command, _BUITLINS_EXIT_, strlen(_BUITLINS_EXIT_)) == 0) {
        builtins_exit(job->processes[0]->cmd);
    }
    else if (strncmp(command, _BUITLINS_FG_, strlen(_BUITLINS_FG_)) == 0) {
        builtins_fg(job->processes[0]->cmd);
    }
    else if (strncmp(command, _BUITLINS_JOBS_, strlen(_BUITLINS_JOBS_)) == 0) {
        builtins_jobs(job->processes[0]->cmd);
    } else {
        return 0;
    }
    return 1;
}

void builtins_bg(Command* cmd) {
    printf("builtins_bg has not been implemented yet!\n");
    exit(0);
}
void builtins_cd(Command* cmd) {
    if (cmd->argc != 2) {
        printf("`cd` takes exactly 1 argument, the path to a directory to switch to\n");
        return;
    }

    chdir(cmd->argv[1]);
}

void builtins_exit(Command* cmd) {
    if (cmd->argc != 1) {
        printf("`exit` does not accept any arguments\n");
        return;
    }
    exit(_EXIT_SUCCESS_);
}

void builtins_fg(Command* cmd) {
    printf("builtins_fg has not been implemented yet!\n");
    exit(0);
}

void builtins_jobs(Command* cmd) {
    Job* job;
    for (int i = 1; i < MAX_JOBS; i++) {
        if (NULL == (job = all_jobs[i])) {
            continue;
        }

        printf("%i: %s\n", i, job->cmd);
    }
}

/////////////////////// END BUILT-IN COMMANDS FUNCTIONS ////////////////////////


//////////////////////////// APPLICATION FUNCTIONS /////////////////////////////

void run_script_file(char const * script_file) {

}

void run_cli() {
    while (true) {
        display_prompt();
        char* command = get_command();
        command[strlen(command) - 1] = '\0';

        Job* job = parse_command(command);

        if(check_builtin(job)) {
            // Release Memory
            free(command);
            job_destroy(job);
            continue;
        }

        // Release memory
        free(command);

        switch(job->n_process) {
            case 0:
                _FAILURE_EXIT_("SOMETHING WENT WRONG\n")
                break;
            case 1:
                dispatch_job(job);
                break;
            default:
                dispatch_piped_jobs(job);
                break;
        }
    }
}


////////////////////////// END APPLICATION FUNCTIONS ///////////////////////////

int main(int argc, char const *argv[]) {
    // set up SIGCHLD
    {
        struct sigaction chld_handler = { .sa_handler = NULL,
                                          .sa_flags = SA_RESTART };

        chld_handler.sa_handler = sigchld_handler;

        // ensure the handler is bound properly
        if (sigaction(SIGCHLD, &chld_handler, NULL) < 0) {
            _FAILURE_EXIT_("FATAL ERROR: Couldn't bind SIGCHLD!\n");
        }
    }

    switch (argc - 1) {
        case 0:
            run_cli();
            break;
        case 1:
            run_script_file(argv[1]);
            break;
        default:
            printf("Usage:\n>>>./wsh [script_file]\n");
            break;
    }
    return 0;
}
