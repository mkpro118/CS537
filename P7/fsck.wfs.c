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
    if (fseek(ps_sb.disk_file, 0, SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        return FSOPFL;
    }

    ps_sb.sb.head = WFS_INIT_ROOT_OFFSET;
    if (fwrite(&ps_sb.sb, sizeof(struct wfs_sb), 1, ps_sb.disk_file) != 1) {
        WFS_ERROR("fwrite failed!\n");
        return FSOPFL;
    }

    for (off_t* off = table; off < &table[ps_sb.n_inodes]; off++) {
        struct wfs_log_entry* entry;

        if ((read_from_disk(*off, &entry) != FSOPSC) || !entry) {
            WFS_ERROR("Failed to read entry at offset %lu "
                      "likely due to a failed system call. ABORTING!\n",
                      *off);
            exit(FSOPFL);
        }

        size_t size = WFS_LOG_ENTRY_SIZE(entry);

        if(fwrite(entry, size, 1, ps_sb.disk_file) < 1) {
            WFS_ERROR("fwrite failed!\n");
            return FSOPFL;
        }

        free(entry);
    }

    ps_sb.sb.head = ftell(ps_sb.disk_file);
    if (fwrite(&ps_sb.sb, sizeof(struct wfs_sb), 1, ps_sb.disk_file) != 1) {
        WFS_ERROR("fwrite failed!\n");
        return FSOPFL;
    }

    end_op();

    WFS_INFO("Finished compacting!\n");
    WFS_INFO("Final file size: %d\n", ps_sb.sb.head);

    fclose(ps_sb.disk_file);
    free(ps_sb.disk_filename);
    free(table);

    return 0;
}
