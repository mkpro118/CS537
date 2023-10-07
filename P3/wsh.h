#ifndef _MK_WSH_
#define _MK_WSH_

#include <sys/types.h>

/* Exit codes */
#define _EXIT_FAILURE_ 1
#define _EXIT_SUCCESS_ 0

/* Built-in commands */
#define _BUITLINS_BG_    "bg"
#define _BUITLINS_CD_    "cd"
#define _BUITLINS_EXIT_  "exit"
#define _BUITLINS_FG_    "fg"
#define _BUITLINS_JOBS_  "jobs"

/* Special characters */
#define _PIPE_ "|"
#define _AMPERSAND_ "&"

/* Token delimiter */
#define _DELIMITER_ " "

/* Shell Prompt */
#define _PROMPT_ "wsh> "

/* String Terminator */
#define _NULL_TERMINATOR_ '\0'

/* Maximum number of jobs */
#define MAX_JOBS 128
#define JOB_START_IDX 1

/* Malloc Check */
#define _MALLOC_CHECK_(x) if (NULL == x) {printf("malloc failed!\n"); exit(1);}

/* Exit on Failure Macro */
#define _FAILURE_EXIT_(msg) { printf(msg); exit(_EXIT_FAILURE_); }

/* Booleans */
typedef enum {
    false,
    true,
} bool;

/* Process States */
typedef enum {
    FOREGROUND,
    BACKGROUND,
    STOPPED,
} JobState;

typedef struct {
    char* cmd;    /* Command name (ex. "ls", "echo") */
    int argc;     /* Number of arguments in the args array */
    char** argv;  /* Array of command arguments (ex. ["ls", "-l", NULL]). Terminated by NULL */
} Command;

typedef struct {
    Command* cmd;          /* The Command struct representing the job */
    pid_t pid;             /* Process ID of the job */
} Process;

typedef struct {
    char* cmd;
    int n_process;         /* Number of Jobs */
    Process** processes;   /* Job ID */
    bool bg;               /* Flag indicating if this command run in the background */
    JobState p_state;  /* State of the job */
} Job;

/* Initializer Functions */
Command* command_init(char*);
Process* process_init(Command*);
Job* job_init(char*, Process**, int, bool);

/* Destructor Functions */
void command_destroy(Command*);
void process_destroy(Process*);
void job_destroy(Job*);

/* Main Loop Functions */
void display_prompt();
char* get_command();
Job* parse_command(char*);
void dispatch_job(Job*);
void dispatch_piped_jobs(Job*);

/* Built-in Commands */
void builtins_bg();
void builtins_cd();
void builtins_exit();
void builtins_fg();
void builtins_jobs();

/* Application Functions */
void run_script(char*);
void run_cli();

#endif
