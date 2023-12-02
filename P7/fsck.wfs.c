#include <stdio.h>
#include <stdlib.h>

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
    free(ps_sb.disk_filename);
    free(table);

    return 0;
}
