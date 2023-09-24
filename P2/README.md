# Project 2

- Name: Mrigank Kumar
- CS Login: mrigank
- WISC ID: 9083537424
- Email: mkumar42@wisc.edu

## INTEGRITY STATEMENT
EXCEPT FOR THE GIVEN SOURCE CODE, ALL OF THE CODE IN THIS PROJECT WAS WRITTEN ENTIRELY BY ME. NO LARGE LANGUAGE MODEL OR OTHER ONLINE OR OFFLINE SOURCE WAS USED FOR THE DEVELOPMENT OR MODIFICATION OF THE SOURCE CODE OF THIS PROJECT.

# Project Implementation and Description.

## Status
The implementation submitted on Saturday, 23rd September 2023, passes all tests defined in the `~cs537-1/tests/P2` directory
- `~cs537-1/tests/P2/initial-xv6/test-getlastcat.sh`: Scores 8/8

## List of Files Changed
- `syscall.c`
- `syscall.h`
- `sysfile.c`
- `user.h`
- `usys.S`
- `Makefile`

## List of Files Added
- `getlastcat.c`

## Implementation Description

This project works by modifying the calls to the `open` and `read` system calls. In both functions, it uses `myproc` to determine which process calls the aforementioned system calls.
This approach was inspired by the following observations:
- If the cat program was with no arguments, then the implementation of `cat.c` uses the read system call with `fd=0`. So if in `sys_read` we know that the current process is `cat` and the first argument as an int is zero, i.e, `fd=0`, then cat has been called with no arguments, and we can copy the relevant message in the buffer.:
- If the cat program is called with one or more arguments, then the implementation of `cat.c` uses the open system call with the path to the file. So if in `sys_read`, we know that the current process is `cat` and the `argstr` call to get the path succeeds, then we can copy that path to the buffer. In case of multiple arguments to `cat`, the latest call to `sys_open` with overwrite the buffer, effectively obtaining the last argument to `cat`. If `sys_open` fails, it returns -1, so in that case, we copy the relevant message to buffer.

When a user uses the `getlastcat` command, we use the user level `int getlastcat(char*)` function to call the `sys_getlastcat` system call to obtain the relevant message from the buffer, copied into the `char*` argument. Then the user level program uses `printf` to print the output.

Below is a list of changes made to individual files.

### 1. `syscall.c`
Added the signature of getlastcat as a system call, and modified the syscalls array to include the new system call.

### 2. `syscall.h`
Added the signature of getlastcast as a system call.

### 3. `sysfile.h`
Modified `sys_read()` and `sys_open()` to track filenames used with the `cat` command.
Added the `sys_getlastcat()` function to copy the contents of the buffer to the given argument.
Added some helper functions for string manipulation, process checking and a global buffer variable to track the history of `cat` arguments
This file reuses the given implementation of `strcpy` and `strcmp` in `ulib.c`, since it is not possible to include `user.h` in this file due to conflicting function signatures.

### 4. `user.h`
Added `getlastcat` as a user level program

### 5. `usys.S`
Added `getlastcast` to the list of `SYSCALL`s

### 6. `Makefile`
Added `_getlastcat` to the list of `UPROGS`
