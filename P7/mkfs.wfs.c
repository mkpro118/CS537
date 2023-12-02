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
 */
int main(int argc, const char* restrict argv[]) {
    if (argc != 2) {
        printf("Usage: mkfs.wfs disk_path\n");
        goto success;
    }

    ps_sb.disk_filename = strdup(argv[1]);
    ps_sb.disk_file = fopen(argv[1], "r+");

    if (!ps_sb.disk_file) {
        WFS_ERROR("Failed to open file \"%s\"!\n", argv[1]);
        goto fail;
    }

    fseek(ps_sb.disk_file, 0, SEEK_SET);

    // Write the Superblock to file
    wfs_sb_init(&ps_sb.sb);

    write_sb_to_disk();

    // Write Log Entry 0, for root to the file
    struct wfs_log_entry entry;
    wfs_inode_init(&entry.inode, DIRECTORY_MODE);

    if (write_to_disk(ftell(ps_sb.disk_file), &entry)) {
        WFS_ERROR("Failed to write to file \"%s\"\n", argv[1]);
        goto fail;
    }

    WFS_INFO("Successfully initialized file \"%s\" for WFS\n", argv[1]);
    fclose(ps_sb.disk_file);
    free(ps_sb.disk_filename);

    success:
    return 0;

    fail:
    WFS_ERROR("Couldn't intialize WFS.\n");
    if (ps_sb.disk_filename)
        free(ps_sb.disk_filename);
    if (ps_sb.disk_file)
        fclose(ps_sb.disk_file);

    return 1;
}
