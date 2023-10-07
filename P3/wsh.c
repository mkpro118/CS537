/**
 * Replicate a simplified version of a Unix Command Line Interpreter/Shell
 *
 * @author Mrigank Kumar
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
// #include <sys/wait.h>
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

    command->cmd = malloc(sizeof(char) * (n + 1));
    _MALLOC_CHECK_(command->cmd)

    strcpy(command->cmd, cmd);

    command->argc = 0;

    int argv_size = 5;
    int argv_incr = 5;

    command->argv = malloc(sizeof(char*) * argv_size);
    _MALLOC_CHECK_(command->argv)

    char* token = strtok(cmd, _DELIMITER_);

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

        token = strtok(NULL, _DELIMITER_);
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

    if (cmd->cmd)
        free(cmd->cmd);

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
    if (NULL != strstr(input, _AMPERSAND_)) {
        bg = true;
    }

    Process* procs[32];
    int n_procs = 0;

    if (NULL != strstr(input, _PIPE_)) {
        char* token = strtok(input, _PIPE_);

        while(NULL != token) {
            Command* cmd = command_init(token);
            if (NULL == cmd) _FAILURE_EXIT_("Command init failed!\n")

            procs[n_procs] = process_init(cmd);

            if (NULL == procs[n_procs]) _FAILURE_EXIT_("Process init failed!\n")

            n_procs++;
        }
    }

    Job* job = job_init(input, procs, n_procs, bg);

    if (NULL == job) _FAILURE_EXIT_("Job init failed!\n")

    return job;
}

void dispatch_job(Job* job) {

    pid_t cpid = fork();

    if (pid < 0) _FAILURE_EXIT_("fork failed!\n")

    switch (cpid) {
        case 0: // child
            char** argv = job->processes[0]->cmd->argv;
            execvp(argv[0], argv);

            _FAILURE_EXIT_("execvp failed!\n")

            break;
        default: // parent
            job->processes[0]->pid = cpid;
            if (job->bg == false) {
                waitpid(cpid);
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
        if (pipe(pipes[i] < 0)) _FAILURE_EXIT_("pipe failed!\n")
    }

    pid_t cpid;
    for (int i = 0; i < job->n_process; i++) {
        cpid = fork();

        if (cpid < 0) _FAILURE_EXIT_("Fork failure\n")

        if (0 == cpid) {

            // close write ends of pipes for previous processes
            if (i > 0) {
                dup2(pipes[i-1][0], stdin);
                close(pipes[i-1][1]);
            }

            // close read ends of pipes for following processes
            if (i < n_pipes) {
                dup2(pipes[i][1], stdout);
                close(pipes[i][0]);
            }

            char** argv = job->processes[i]->cmd->argv;

            execvp(argv[0], argv);

            _FAILURE_EXIT_("execvp failed!\n");
        } else {
            job->processes[i]->pid = cpid;
        }
    }

    // Close all remaining pipe file descriptors
    for (int i = 0; i < n_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if (job->bg == false) {
        waitpid(cpid);
    } else {
        job->p_state = BACKGROUND;
    }
}


/////////////////////////// END MAIN LOOP FUNCTIONS ////////////////////////////


///////////////////////// BUILT-IN COMMANDS FUNCTIONS //////////////////////////

void check_builtin(char* command) {
    if (strcmp(command, _BUITLINS_BG_) == 0) {
        builtins_bg();
    }
    else if (strcmp(command, _BUITLINS_CD_) == 0) {
        builtins_cd();
    }
    else if (strcmp(command, _BUITLINS_EXIT_) == 0) {
        builtins_exit();
    }
    else if (strcmp(command, _BUITLINS_FG_) == 0) {
        builtins_fg();
    }
    else if (strcmp(command, _BUITLINS_JOBS_) == 0) {
        builtins_jobs();
    }
}

void builtins_bg();
void builtins_cd();

void builtins_exit() {
    exit(_EXIT_SUCCESS_);
}

void builtins_fg();
void builtins_jobs();

/////////////////////// END BUILT-IN COMMANDS FUNCTIONS ////////////////////////


//////////////////////////// APPLICATION FUNCTIONS /////////////////////////////

void run_script_file(char* script_file) {

}

void run_cli() {
    while (true) {
        display_prompt();
        char* command = get_command();

        check_builtin(command);

        Job* job = parse_command(command);

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
