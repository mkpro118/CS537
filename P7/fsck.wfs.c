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
    #if WFS_DBUG == 1
    if (argc != 3) {
        printf("IN DEBUG MODE\n");
        printf("Usage: $ fsck.wfs disk_path outfile\n");
        return 0;
    }
    #else
    if (argc != 2) {
        printf("Usage: $ fsck.wfs disk_path\n");
        return 0;
    }
    #endif

    wfs_init(argv[1]);

    if (ps_sb.n_inodes < 1) {
        WFS_INFO("No inodes were found.\n"
                 "\"%s\" is possibly an invalid disk file.\n"
                 "Aborting.\n", argv[1]);
        return 0;
    }

    WFS_INFO("Initial file size: %d\n", ps_sb.sb.head);

    heap_sort(ps_sb.itable.table, ps_sb.n_inodes);

    #if WFS_DBUG == 1
    FILE* infile = ps_sb.disk_file;
    FILE* outfile = fopen(argv[2], "r+");
    if (!outfile) {
        WFS_ERROR("Couldn't open outfile \"%s\"\n", ps_sb.disk_filename);
        exit(FSOPFL);
    }

    ps_sb.disk_file = outfile;
    #endif

    #ifndef WFS_DBUG
    begin_op();
    #endif

    ps_sb.sb.head = WFS_INIT_ROOT_OFFSET;
    write_sb_to_disk();

    off_t* table = ps_sb.itable.table;

    for (off_t* off = table; off < &table[ps_sb.n_inodes]; off++) {
        struct wfs_log_entry* entry;

        #if WFS_DBUG == 1
        ps_sb.disk_file = infile;
        #endif

        read_from_disk(*off, &entry);

        if (!entry) {
            WFS_ERROR("Failed to read entry at offset %lu "
                      "likely due to a failed system call. ABORTING!\n",
                      *off);
            exit(FSOPFL);
        }

        #if WFS_DBUG == 1
        ps_sb.disk_file = infile;
        #endif

        if(append_log_entry(entry)) {
            WFS_ERROR("Ran out of space while compacting? ABORTING!\n");
            exit(FSOPFL);
        }
    }

    write_sb_to_disk();

    #ifndef WFS_DBUG
    end_op();
    #endif

    WFS_INFO("Finished compacting!\n");
    WFS_INFO("Final file size: %d\n", ps_sb.sb.head);

    fclose(ps_sb.disk_file);
    free(ps_sb.disk_filename);

    return 0;
}
