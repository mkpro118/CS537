#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __unix__
#define FUSE_USE_VERSION 30
#include <fuse.h>
#endif

#include "wfs.h"

///////////////////////// FUSE HANDLER FUNCTIONS START /////////////////////////

static void wfs_stat_init(struct stat* stbuf, struct wfs_inode* restrict inode) {
    *stbuf = (struct stat) {
        .st_ino   = inode->inode_number,
        .st_mode  = inode->mode,
        .st_nlink = inode->links,
        .st_uid   = inode->uid,
        .st_gid   = inode->gid,
        .st_size  = inode->size,
        .st_atime = inode->atime,
        .st_mtime = inode->mtime,
        .st_ctime = inode->ctime,
    };
}

static int wfs_getattr(const char* path, struct stat* stbuf) {
    _check();
    uint inode_number;

    if(parse_path(path, &inode_number) != FSOPSC) {
        WFS_ERROR("Failed to find inode\n");
        return -ENOENT;
    }

    struct wfs_log_entry* entry = get_log_entry(inode_number);
    if (!entry) {
        WFS_ERROR("Failed to find log_entry for inode %d.\n", inode_number);
        return -ENOENT;
    }

    wfs_stat_init(stbuf, &entry->inode);

    free(entry);
    return 0;
}

static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
    return 0;
}

static int wfs_mkdir(const char* path, mode_t mode) {
    return 0;
}

static int wfs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    if (size == 0)
        goto done;

     _check();
    uint inode_number;

    if(parse_path(path, &inode_number) != FSOPSC) {
        WFS_ERROR("Failed to find inode\n");
        return -ENOENT;
    }

    struct wfs_log_entry* entry = get_log_entry(inode_number);
    if (!entry) {
        WFS_ERROR("Failed to find log_entry for inode %d.\n", inode_number);
        return -ENOENT;
    }

    if (_check_reg_inode(&entry->inode))
        return -EACCES;

    // Read sizebytes from the given file into the buffer buf, 
    // beginning offset bytes into the file.
    char* data = entry->data;
    int file_size = entry->inode.size;
    int n_bytes = 0;

    if (offset >= file_size)
        goto done;

    n_bytes = offset + size <= file_size ? size : file_size - offset;

    // Returns the number of bytes transferred, 
    // or 0 if offset was at or beyond the end of the file.

    data = data + offset;
    memmove(buf, data, n_bytes);

    free(entry);

    done:
    return n_bytes;
}

static int wfs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return 0;
}

static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    filler(buf,  ".", NULL, 0); // Current  directory
    filler(buf, "..", NULL, 0); // Previous directory
    /*
    _check();
    uint inode_number;

    if(parse_path(path, &inode_number) != FSOPSC) {
        WFS_ERROR("Failed to find inode\n");
        return -ENOENT;
    }

    struct wfs_log_entry* entry = get_log_entry(inode_number);
    if (!entry) {
        WFS_ERROR("Failed to find log_entry for inode %d.\n", inode_number);
        return -ENOENT;
    }

    // now we have access to the data we want to keep going copying until end of 
    int n_entries = entry->inode.size / sizeof(struct wfs_dentry);

    struct wfs_dentry* dentry = (struct wfs_dentry*) entry->data;
    for (int i = 0; i < n_entries; i++, dentry++) {
        // copy over stat pass it in 
        // char* filename = dentry->name;
        inode_number = dentry->inode_number;
        break;
    } */
    return 0;
}

static int wfs_unlink(const char* path) {
    return 0;
}

////////////////////////// FUSE HANDLER FUNCTIONS END //////////////////////////


static struct fuse_operations ops = {
    .getattr = wfs_getattr,
    .mknod   = wfs_mknod,
    .mkdir   = wfs_mkdir,
    .read    = wfs_read,
    .write   = wfs_write,
    .readdir = wfs_readdir,
    .unlink  = wfs_unlink,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: $ mount.wfs [FUSE options] disk_path mount_point\n");
        return 0;
    }

    ps_sb.disk_filename = strdup(argv[argc - 2]);
    if(!ps_sb.disk_filename){
        WFS_ERROR("strdup failed\n");
        exit(ITOPFL);
    }
    WFS_INFO("disk_path = %s\n", argv[argc - 2]);
    ps_sb.disk_file = fopen(ps_sb.disk_filename, "a+");

    if (!ps_sb.disk_file) {
        WFS_ERROR("Couldn't open file \"%s\"\n", ps_sb.disk_filename);
        exit(ITOPFL);
    }

    invalidate_itable();
    invalidate_path_history();

    validate_disk_file();

    if (!ps_sb.is_valid) {
        WFS_ERROR("Disk File validation failed! (disk_file: \"%s\")\n",
                  ps_sb.disk_filename);
        exit(FSOPFL);
    }

    setup_flock();

    WFS_INFO("Building I-Table...\n");

    int err;
    if((err = build_itable(ps_sb.disk_file)) != ITOPSC) {
        WFS_ERROR("Failed to build I-Table | Error Code: %d\n", err);
        exit(FSOPFL);
    }

    WFS_INFO("Built I-Table successfully!\n");
    WFS_INFO("Parsed %d log entries, with %d inodes\n", ps_sb.n_log_entries, ps_sb.n_inodes);

    WFS_DEBUG("%p\n", (void*)&ops);

    /* For testing */
    #if WFS_DBUG == 1

    int print_inodes = 0;
    int test_parse   = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp("-i", argv[i]) == 0) {
            print_inodes = 1;
        }
        else if (strcmp("-p", argv[i]) == 0) {
            test_parse = 1;
        }
    }

    if (print_inodes) {
        for (uint i = 0; i < ps_sb.n_inodes; i++) {
            struct wfs_log_entry* entry = get_log_entry(i);
            if (entry) {
                PRINT_LOG_ENTRY(entry);

                free(entry);
            }
        }
    }

    if (test_parse) {
        if (strstr(argv[argc - 2], "prebuilt_disk") == NULL) {
            printf("-p required disk_file to be \"prebuilt_disk\"\n");
            goto done;
        }

        const char* paths_to_check[] = {
            "/file0", "/file1", "/dir0", "/dir1",
            "///file0", "/file1///", "///dir0/", "/dir1///",
            "/dir0/../file0", "/./dir1/../file1", "/dir1/../dir0", "/./dir0/../dir1//",
            "/dir0/../file0", "/./file1", "/dir1/../dir0", "/./dir0/../dir1//",
            "/dir0/file00", "/dir0/file01", "/dir1/file10", "/dir1/file11",
            "///dir0/file00", "/dir1/../dir0/file01", "/dir1/../dir1/file10", "/./dir0/./../dir1/file11",
            "/dir1/./../dir0/file00///", "///dir0/file01", "//dir1/file10", "/./dir0/../dir1/file11",
            "mk", "saanvi", "//kumar//", "\\malhotra\\",
        };

        const uint expected_inodes[] = {
             1,  2,  3,  4,
             1,  2,  3,  4,
             1,  2,  3,  4,
             1,  2,  3,  4,
             5,  6,  8,  7,
             5,  6,  8,  7,
             5,  6,  8,  7,
            -1, -1, -1, -1,
        };

        int n_paths = sizeof(paths_to_check) / sizeof(char*);
        int n_exp = sizeof(expected_inodes) / sizeof(uint);

        if (n_exp != n_paths) {
            printf("Inconsistents tests! %d != %d\n", n_paths, n_exp);
            goto done;
        }

        printf("about to run %i tests.\n", n_exp);

        for (int j = 0; j < n_exp; j++) {
            const char* p = paths_to_check[j];
            uint i;

            printf("|Test %2i|\tPath:\t%s\n", j+1, p);

            if((parse_path(p, &i) != FSOPSC) && (j < n_exp - 4 )) {
                printf("lol parse path failed\n");
                exit(1);
            }

            printf("Expected: %u\tActual: %u\t", expected_inodes[j], i);
            if (i == expected_inodes[j]) {
                if (j < (n_exp - 4))
                    printf("\t\t\t| SUCCESS\n");
                else
                    printf("\t| SUCCESS\n");
            } else {
                if (j < (n_exp - 4))
                    printf("\t\t\t| FAIL\n");
                else
                    printf("\t| FAIL\n");
            }
            printf("\n");
        }
    }

    done:
    return 0;
    #else
    int fuse_argc = argc - 1;

    argv[argc - 2] = argv[fuse_argc];
    argv[fuse_argc] = NULL;

    WFS_DEBUG("%p\n", (void*)&ops);
    return fuse_main(fuse_argc, argv, &ops, NULL);
    #endif
}
