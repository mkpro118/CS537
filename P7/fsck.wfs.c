#include <stdio.h>
#include <stdlib.h>

#include "wfs.h"

#include <stdio.h>

static inline void swap(off_t *a, off_t *b) {
    off_t t = *a;
    *a = *b;
    *b = t;
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

    #if WFS_DBUG == 1
    for (int i = 0; i < ps_sb.n_inodes; i++)
        printf("%lu\n", table[i]);
    #endif

    begin_op();

    ps_sb.sb.head = WFS_INIT_ROOT_OFFSET;
    write_sb_to_disk();

    for (off_t* off = table; off < &table[ps_sb.n_inodes]; off++) {
        WFS_DEBUG("Current Head = %u\n", ps_sb.sb.head);

        struct wfs_log_entry* entry;

        WFS_DEBUG("Reading from offset %lu\n", *off);
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

        //ps_sb.sb.head += WFS_LOG_ENTRY_SIZE(entry);
        WFS_DEBUG("New Head = %i\n", ps_sb.sb.head);

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
