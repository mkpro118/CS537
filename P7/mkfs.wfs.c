#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define WFS_SETUP
#include "wfs.h"

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
int main(int argc, const char* restrict argv[]) {
    if (argc != 2) {
        printf("Usage: mkfs.wfs disk_path\n");
        goto success;
    }

    // filename = argv[1]

    // Write in binary mode
    FILE* img_file = fopen(argv[1], "wb");

    if (!img_file) {
        WFS_ERROR("Failed to open file \"%s\"!\n", argv[1]);
        goto fail;
    }

    // Write the Superblock to file
    struct wfs_sb sb;
    wfs_sb_init(&sb);

    size_t sb_size = sizeof(struct wfs_sb);

    if (fwrite(&sb, sb_size, 1, img_file) != 1) {
        WFS_ERROR("Failed to write to file \"%s\"\n", argv[1]);
        goto fail;
    }

    // Write Log Entry 0, for root to the file
    struct wfs_log_entry entry;
    wfs_inode_init(&entry.inode, DIRECTORY_MODE);

    size_t entry_size = sizeof(struct wfs_log_entry);

    if (fwrite(&entry, entry_size, 1, img_file) != 1) {
        WFS_ERROR("Failed to write to file \"%s\"\n", argv[1]);
        goto fail;
    }

    printf("Successfully initialized file \"%s\" for WFS\n", argv[1]);
    fclose(img_file);

    success:
    return 0;

    fail:
    WFS_ERROR("Couldn't intialize WFS.\n");
    return 1;
}
