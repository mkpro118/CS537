# Project 5

- Names: Mrigank Kumar, Saanvi Malhotra
- CS Logins: mrigank, malhotra
- WISC IDs: 9083537424, 9083552423
- Emails: mkumar42@wisc.edu, smalhotra9@wisc.edu

## INTEGRITY STATEMENT
EXCEPT FOR THE GIVEN SOURCE CODE, ALL OF THE CODE IN THIS PROJECT WAS WRITTEN ENTIRELY BY US. NO PART OF ANY MODIFIED FILES OR NEW FILES WAS SOURCED FROM EITHER A LARGE LANGUAGE MODEL OR THE INTERNET. OUR IMPLEMENTATION DRAWS LARGE INSPIRATIONS FROM CERTAIN FUNCTIONS IN THE FILE `vm.c`.

# Project Implementation and Description.

## Status
The implementation submitted on Tuesday, 7th October 2023, passes all tests defined in the `~cs537-1/tests/P5` directory
- `~cs537-1/tests/P5/runtests`: Scores 16/16 (15 points out of 15)

## List of Files Changed
- `defs.h`
- `proc.c`
- `proc.h`
- `syscall.c`
- `syscall.h`
- `sysfile.c`
- `user.h`
- `usys.S`
- `trap.c`
- `vm.c`

## List of Files Added
- `mmap.h`

## Implementation Description
We implemented two main system call functions- `sys_mmap` and `sys_munmap`.

To help with our implementation, we made a struct called mmap which can be found in the mmap.h file and we made changes to proc.h and altered the process struct to include an array of 32 mmap structs. This ensures that each process has an an array of 32 mmap structs that we will later utilize in our implementation of the `sys_mmap` function call.

In `sys_mmap`, the function first verifies the validity of the arguments we take in and handles errors if any of them are invalid. It then searches for an available slot in the process's mmap array to store information about the memory mapping. First we check if the MAP_FIXED flag is set. If its not, we attempt to find a suitable address range in the process's address space, ensuring that it doesn't overlap with existing mappings and falls within the valid memory address range. If MMAP_FIXED is set, then we check the validity of the address given to us and check to make sure it doesn't potentially overlap with existing mappings. If all checks pass, we initialize a data structure within our mmaps array to represent the memory mapping and return the starting address of the mapping. If grows up is set for either case, we add another page to be a guard page. We also set the refcount for that specific mmap struct to be 1 because we have at least one exisitng mmap. `refcount` is a field within our mmap struct which will help us later to manage parent and child processes sharing an address space. If at any point a check fails or something goes wrong, we return -1 to indicate failure.

In `sys_munmap`, we deallocate memory regions previously created by `sys_mmap`. For parameters, we take in the starting address of the mapping and the length to be unmapped. We first test the validity of these parameters and then search for the mmap entry corresponding to the given address in the process's mmap array. If we find a valid mmap entry that includes the specified address, we proceed with the unmapping process; otherwise, we return -1. Our unmapping process is as follows - Depending on whether the mapping is anonymous or private, we either free the memory without writing to a file or write the mapped data back to the file. We then calculate the length of memory to deallocate and free the associated physical pages in the page table. Finally, we clear the mmap entry and return 0 to indicate success. If any part of the unmapping process fails, we return -1 to indicate failure.

In the `trap` function, when there is a pagefault, (in case `T_PGFLT`), we check if there is a page we need to allocate. First we get the address at which the page fault occured by using the `rcr2` function. Then, we iterate through all the mmap structs in the process where the page fault occured to check and see if the fault address we got matches any of the mappings we have. If it does, then we manage the page fault in the following manner. First, we check the page table entry to see if the physical page exists. If it doesn't, it means its time to allocate that page table entry. We also check here if the `MAP_GROWSUP` flag is set and if it is, we check to see if the fault address is within the guard page. If it is, then we allocate a new guard page if it is possible to do so. If none of the above conditions are met, we seg fault and kill the process.

Finally, we modified `fork` and `exit` mainly by utilizing a field in our mmap struct called refcount. In fork, we made sure to increase refcount and copy mappings from parent to child every time a fork occured. If `MAP_PRIVATE` was set, we duplicate physical pages, and copy parent's data. If `MAP_SHARED` is set, we ensure that if a parent and child are sharing a memory mapping, if the child terminates and the parent is still running, we don't delete that memory mapping because the parent/maybe other child processes still need it. We make sure to do this by not freeing physical pages unless refcount is 0.


Below is a list of changes made to individual files.
### 1. `defs.h`
- Added function prototypes for `mappages` and `walkpgdir`.

### 2. `proc.c`
- Modified the `fork` and `exit` functions. In fork we copied mappings from parent to child and in exit we removed mappings dependent on if all child and parent processes were done.

### 3. `proc.h`
- Extended the `proc` structure to include an array of 32 mmap structures.

### 4. `syscall.c`
- Extended the `syscalls` array to contain `sys_mmap` and `sys_munmap`.

### 5. `syscall.h`
- Added `SYS_mmap` macro.
- Added `SYS_munmap` macro.

### 6. `sysfile.c`
- Implemented `sys_mmap`.
- Implemented `sys_munmap`.

### 7. `user.h`
- Added user level function prototypes for the `mmap` and `munmap` system calls.

### 8. `usys.S`
- Added `SYSCALL(mmap)` and `SYSCALL(munmap)`.

### 9. `trap.c`
- Added code to handle allocation in the pagefault case.

### 10. `vm.c`
- Removed the `static` keywords from `mappages` and `walkpgdir` to make them publicly available.
