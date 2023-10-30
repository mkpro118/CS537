# Project 2

- Name: Mrigank Kumar
- CS Login: mrigank
- WISC ID: 9083537424
- Email: mkumar42@wisc.edu

## INTEGRITY STATEMENT
EXCEPT FOR THE GIVEN SOURCE CODE, ALL OF THE CODE IN THIS PROJECT WAS WRITTEN ENTIRELY BY ME. CERTAIN IDEAS, SUCH AS THE `last_update` FLAG WAS INPSIRED BY AN INSTRUCTOR WHOSE IDENTITY IS NOT KNOWN TO ME. THE CORE LOGIC WAS DEVELOPED WITH THE ASSISTANCE OF JOHN SHAWGER. NO PART OF ANY MODIFIED FILES OR NEW FILES WAS SOURCED FROM EITHER A LARGE LANGUAGE MODEL OR THE INTERNET.

# Project Implementation and Description.

## Status
The implementation submitted on Tuesday, 24th October 2023, passes all tests defined in the `~cs537-1/tests/P4` directory
- `~cs537-1/tests/P2/runtests`: Scores 15/15 (14 points out of 14)

## List of Files Changed
- `proc.c`
- `proc.h`
- `syscall.c`
- `syscall.h`
- `sysfile.c`
- `sysproc.c`
- `user.h`
- `usys.S`
- `Makefile`

## List of Files Added
- `psched.h`

## Implementation Description
The modified scheduler function works by calculating priorities every 100 ticks, then finding processes with the lowest `priority` value, and running those processes in a Round Robin manner.

The scheduler loops over the processes in the process table, finding the minimum `priority` value. Then, it loops over the process table again in order to execute processes that are `RUNNABLE`, and have their `priority` field equal to the minimum priority determined in the previous step. If a process does not satisfy the above conditions, we skip executing that process in the current tick.

If a process does satisfy the above conditions, we execute the process, and increment its `ticks` and `cpu` usage fields.

After every process is either skipped or a timer interrupt occurs, and a 100 ticks have passed since the last time priorities were calculated, the priorities of every process is recalculated. Each process' cpu usage is decayed to reward processes that haven't used the CPU recently.


Below is a list of changes made to individual files.

### 1. `proc.c`
- Modified the `scheduler` function to behave as a Multi-Level Feedback Queue Scheduler
- Modified the `wakeup1` function to only wakeup processes that have no more `sleep_ticks` remaining
- Modified the `allocproc` function to set default values to additional fields
- Added functions to calculate priorities of processes

### 2. `proc.h`
- Extended the `proc` structure with the following fields
```c
struct proc {
  // Original fields

  int priority;    // Process priority
  int nice;        // Niceness of the process
  int ticks;       // Ticks
  int cpu;         // CPU Usage
  int sleep_ticks; // Sleep ticks
};
```

### 3. `syscall.c`
- Extended the `syscalls` array to contain `sys_nice` and `sys_getschedstate`

### 4. `syscall.h`
- Added `SYS_nice` macro
- Added `SYS_getschedstate` macro

### 5. `sysfile.c`
- Implemented `sys_nice`
- Implemented `sys_getschedstate`

### 6. `sysproc.c`
- Set `sleep_ticks` in `sys_sleep`

### 7. `user.h`
- Added user level function prototypes for the `nice` and `getschedstate` system calls

### 8. `usys.S`
- Added `SYSCALL(nice)` and `SYSCALL(getschedstate)`

### 9. `Makefile`
- Changed the CPU variable to run xv6 on a single cpu
- Changed the CFLAGS to disable optimizations
