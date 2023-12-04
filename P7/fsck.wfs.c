#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "wfs.h"

/**
 * Performs a swap of the contents at the given addresses
 *
 * @param a address of first value to swap
 * @param b address of second value to swap
 */
static inline void swap(off_t *x, off_t *y) {
    off_t t = *x;
    *x = *y;
    *y = t;
}

/**
 * Enforces a min heap property on the given array
 *
 * @param arr The address of the first element of the array to heapify
 * @param len The length of the array
 * @param idx The index of element to consider as root
 */
static void heapify(off_t* arr, int len, int idx) {
    int left, right;
    int max = idx;

    do {
        left = (idx << 1) + 1;
        right = left + 1;

        if (left < len && arr[left] > arr[max])
            max = left;

        if (right < len && arr[right] > arr[max])
            max = right;

        if (max != idx) {
            swap(arr + idx, arr + max);
            idx = max;
        } else {
            break;
        }
    } while(1);
}

/**
 * Sorts an array in-place using the Heap Sort Algorithm
 *
 * @param arr The address of the first element in the array to sort
 * @param n   The size of the array
 */
static void heap_sort(off_t* arr, int n) {
    // Heapify for nodes with children
    for (int i = n / 2 - 1; i >= 0; i--)
        heapify(arr, n, i);

    // Extract max and set to the end
    for (int i = n - 1; i >= 0; i--) {
        swap(arr, arr + i);
        heapify(arr, i, 0);
    }
}

/**
 * Compacts the WFS by removing redundant entries
 *
 * Takes in one argument, which is the disk image to compact
 *
 * Usage:
 *    $: fsck.wfs <filename>
 */
int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf("Usage: $ fsck.wfs disk_path\n");
        return 0;
    }

    wfs_init(argv[0], argv[1]);

    int ret;
    do {
        ps_sb.fsck_lock.l_type = F_WRLCK;
        ps_sb.fsck_lock.l_pid = 0;
        ret = fcntl(fileno(ps_sb.disk_file), F_GETLK, &ps_sb.wfs_lock);
    } while (errno == EINTR);

    if (ret != -1) {
        WFS_ERROR("FLock Lock should have failed! (err: %s). Aborting\n",
                  strerror(errno));
        exit(FSOPFL);
    }

    pid_t pid = ps_sb.fsck_lock.l_pid;

    if (pid == getpid()) {
        WFS_ERROR("Failed to get the server's pid. Aborting\n");
        exit(FSOPFL);
    }

    if (kill(pid, SIGUSR1) < 0) {
        WFS_ERROR("FATAL ERROR: Couldn't send user1 signal!\n");
        exit(FSOPFL);
    }

    acquire_fsck_lock();

    if (ps_sb.n_inodes < 1) {
        WFS_INFO("No inodes were found.\n"
                 "\"%s\" is possibly an invalid disk file.\n"
                 "Aborting.\n", argv[1]);
        return 0;
    }

    WFS_INFO("Initial file size: %d\n", ps_sb.sb.head);

    off_t* table = malloc(sizeof(off_t) * ps_sb.n_inodes);

    if (!table) {
        WFS_ERROR("Malloc failed while compacting!\n");
        exit(1);
    }

    memcpy(table, ps_sb.itable.table, sizeof(off_t) * ps_sb.n_inodes);

    heap_sort(table, ps_sb.n_inodes);

    begin_op();

    ps_sb.sb.head = WFS_INIT_ROOT_OFFSET;
    write_sb_to_disk();

    for (off_t* off = table; off < &table[ps_sb.n_inodes]; off++) {
        struct wfs_log_entry* entry;

        if ((read_from_disk(*off, &entry) != FSOPSC) || !entry) {
            WFS_ERROR("Failed to read entry at offset %lu "
                      "likely due to a failed system call. ABORTING!\n",
                      *off);
            exit(FSOPFL);
        }

        if(append_log_entry(entry)) {
            WFS_ERROR("Ran out of space while compacting? ABORTING!\n");
            exit(FSOPFL);
        }

        free(entry);
    }

    write_sb_to_disk();

    end_op();

    WFS_INFO("Finished compacting!\n");
    WFS_INFO("Final file size: %d\n", ps_sb.sb.head);

    fclose(ps_sb.disk_file);

    release_fsck_lock();
    if (kill(pid, SIGUSR2) < 0) {
        WFS_ERROR("FATAL ERROR: Couldn't send user1 signal!\n");
        exit(FSOPFL);
    }

    free(ps_sb.disk_filename);
    free(table);

    return 0;
}
