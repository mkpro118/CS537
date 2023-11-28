#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define WFS_SETUP
#include "wfs.h"

#define WFS_N_ITEMS 1

/**
 * Clears the given file of any data, writes out the superblock
 * and initializes the root directory
 * 
 * Takes in one argument, which is the file to use as logbook
 * 
 * Usage:
 *    $: mkfs.wfs <filename>
 * 
 * Status: Write Superblock is done
 *         TODO: Initialize root directory
 */
int main(int argc, const char * argv[]) {
    if (argc != 2) {
        printf("Usage: mkfs.wfs <img_file>\n");
        goto success;
    }

    // filename = argv[1]

    // Write in binary mode
    FILE* img_file = fopen(argv[1], "wb");

    if (!img_file) {
        fprintf(stderr, "Failed to open file \"%s\"!\n", argv[1]);
        goto fail;
    }

    // Write the Superblock to file
    struct wfs_sb sb;
    wfs_sb_init(&sb);

    size_t sb_size = sizeof(struct wfs_sb);

    if (fwrite(&sb, sb_size, WFS_N_ITEMS, img_file) != WFS_N_ITEMS) {
        fprintf(stderr, "Failed to write to file \"%s\"\n", argv[1]);
        goto fail;
    }

    // Write Log Entry 0, for root to the file
    struct wfs_log_entry entry;
    wfs_inode_init(&entry.inode);

    size_t entry_size = sizeof(struct wfs_log_entry);

    if (fwrite(&entry, entry_size, WFS_N_ITEMS, img_file) != WFS_N_ITEMS) {
        fprintf(stderr, "Failed to write to file \"%s\"\n", argv[1]);
        goto fail;
    }

    fprintf(stderr, "Successfully initialized file \"%s\" for WFS\n", argv[1]);

    success:
    return 0;

    fail:
    perror("FATAL ERROR: Couldn't intialize WFS.\n");
    return 1;
}
