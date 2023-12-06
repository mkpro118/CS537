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
        return 0;
    }

    ps_sb.disk_filename = strdup(argv[1]);
    ps_sb.disk_file = wfs_fopen(argv[1], "r+");

    if (!ps_sb.disk_file) {
        WFS_ERROR("Failed to open file \"%s\"!\n", argv[1]);
        goto fail;
    }

    set_max_file_size();
    ps_sb.rebuilding = 1;
    begin_op();

    wfs_fseek(ps_sb.disk_file, 0, SEEK_SET);

    // Write the Superblock to file
    wfs_sb_init(&ps_sb.sb);

    if (write_sb_to_disk() != FSOPSC) {
        WFS_ERROR("Failed to write superblock to file \"%s\"\n", argv[1]);
        goto fail;
    }

    // Write Log Entry 0, for root to the file
    struct wfs_log_entry entry;
    wfs_inode_init(&entry.inode, DIRECTORY_MODE | 777);

    if (write_to_disk(WFS_INIT_ROOT_OFFSET,  &entry) != FSOPSC) {
        WFS_ERROR("Failed to write to file \"%s\"\n", argv[1]);
        goto fail;
    }

    WFS_INFO("Successfully initialized file \"%s\" for WFS\n", argv[1]);

    end_op();
    wfs_fclose(ps_sb.disk_file);
    free(ps_sb.disk_filename);
    return 0;

    fail:
    end_op();
    WFS_ERROR("Couldn't intialize WFS.\n");
    if (ps_sb.disk_filename)
        free(ps_sb.disk_filename);
    if (ps_sb.disk_file)
        wfs_fclose(ps_sb.disk_file);

    return 1;
}
