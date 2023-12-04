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
    _check();
    return 0;
}

static int wfs_mkdir(const char* path, mode_t mode) {
    _check();
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
    // now we copy from buff to entry 

    // check if its too much- offset + size > inode.size 
    if(offset + size > entry->inode.size){
        // realloc 

        // change inode size if u realloc-ed
    }

    
    free(entry);
    return 0;
}

static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    _check();

    filler(buf,  ".", NULL, 0); // Current  directory
    filler(buf, "..", NULL, 0); // Previous directory
    
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

    int n_entries = entry->inode.size / sizeof(struct wfs_dentry);

    struct wfs_dentry* dentry = (struct wfs_dentry*) entry->data;

    for (int i = 0; i < n_entries; i++, dentry++) {
        inode_number = dentry->inode_number;
        struct wfs_log_entry* entry = get_log_entry(inode_number);

        if (!entry) {
            WFS_ERROR("Failed to find log_entry for inode %u.\n", inode_number);
            continue;
        }

        struct stat stbuf;
        wfs_stat_init(&stbuf, &entry->inode);

        filler(buf, dentry->name, &stbuf,0);
        free(entry);
    }

    free(entry);
    return 0;
}

static int wfs_unlink(const char* path) {
    _check();

    uint inode_number;

    if(parse_path(path, &inode_number) != FSOPSC) {
        WFS_ERROR("Failed to find inode\n");
        return -ENOENT;
    }

    if(inode_number == 0){
        WFS_ERROR("Cannot unlink root (%s)\n", path);
        return -1;
    }

    struct wfs_log_entry* entry = get_log_entry(inode_number);

    if (!entry) {
        WFS_ERROR("Failed to find log_entry for inode %d.\n", inode_number);
        return -ENOENT;
    }

    if(_check_reg_inode(&entry->inode)){
        WFS_ERROR("Unlink is only supported on regular files\n");
        return -1;
    }

    entry->inode.deleted = 1;

    if (inode_number > ps_sb.n_inodes || inode_number >= ps_sb.itable.capacity) {
        WFS_INFO("Inode %d was not found in the I-Table. Rebuilding...\n", inode_number);
        invalidate_itable();
        if (build_itable() != ITOPSC) {
            WFS_ERROR("Failed to re-build I-Table.\n");
            exit(ITOPFL);
        }
        WFS_INFO("I-Table was re-built successfully\n");
    }

    off_t offset = ps_sb.itable.table[inode_number];

    write_to_disk(offset, entry);
    free(entry);

    // Change the parent directory to reflect the deletion
    char* _path = simplify_path(path);

    char* base_file = strrchr(_path, '/');

    struct wfs_dentry dentry = {
        .name = {0},
        .inode_number = inode_number,
    };

    // Special Case, File is in root
    if (!base_file) {
        if (!strncpy(dentry.name, base_file, MAX_FILE_NAME_LEN)) {
            WFS_ERROR("strncpy failed!\n");
            return -1;
        }

        entry = get_log_entry(0);

        if (!entry) {
            WFS_ERROR("Failed to find log_entry for inode %d.\n", inode_number);
            return -ENOENT;
        }

        if (remove_dentry(&entry, &dentry) != FSOPSC) {
            WFS_ERROR("Failed to remove dentry %s from inode %i\n", _path, 0);
            return -1;
        }

        free(_path);
        return 0;
    }

    *base_file = 0;

    if (!strncpy(dentry.name, base_file + 1, MAX_FILE_NAME_LEN)) {
        WFS_ERROR("strncpy failed!\n");
        return -1;
    }

    // Find parent Inode
    if(parse_path(_path, &inode_number) != FSOPSC) {
        WFS_ERROR("Failed to find inode\n");
        return -ENOENT;
    }

    entry = get_log_entry(inode_number);

    if (!entry) {
        WFS_ERROR("Failed to find log_entry for inode %d.\n", inode_number);
        return -ENOENT;
    }

    if (remove_dentry(&entry, &dentry) != FSOPSC) {
        WFS_ERROR("Failed to remove dentry %s from inode %i\n", _path, 0);
        return -1;
    }



    free(_path);
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

    wfs_init(argv[0], argv[argc - 2]);

    WFS_DEBUG("%p\n", (void*)&ops);

    /* For testing */
    #if WFS_DBUG == 1

    int print_inodes = 0;
    int test_parse   = 0;
    int add          = 0;
    int remove       = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp("-i", argv[i]) == 0) {
            print_inodes = 1;
        } else if (strcmp("-p", argv[i]) == 0) {
            test_parse = 1;
        } else if (strcmp("-a", argv[i]) == 0) {
            add = 1;
        } else if (strcmp("-r", argv[i]) == 0) {
            remove = 1;
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
            printf("-p requires disk_file to be \"prebuilt_disk\"\n");
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
                    printf("\t\t\t| \x1b[32mSUCCESS\x1b[0m\n");
                else
                    printf("\t| \x1b[32mSUCCESS\x1b[0m\n");
            } else {
                if (j < (n_exp - 4))
                    printf("\t\t\t| \x1b[31mFAIL\x1b[0m\n");
                else
                    printf("\t| \x1b[31mFAIL\x1b[0m\n");
            }
            printf("\n");
        }
    }

    if (add) {
        if (strstr(argv[argc - 2], "prebuilt_disk") == NULL) {
            printf("-a requires disk_file to be \"prebuilt_disk\"\n");
            goto done;
        }

        struct wfs_log_entry* root = get_log_entry(0);

        if (!root) {
            WFS_ERROR("Failed to fetch root entry\n");
            goto done;
        }

        struct wfs_dentry dentry = {
            .name = "file3",
            .inode_number = ps_sb.n_inodes,
        };

        if (add_dentry(&root, &dentry)) {
            WFS_ERROR("Failed to add dentry! {.name = %s, .inode_number = %lu}\n", dentry.name, dentry.inode_number);
        }

        ps_sb.n_inodes++;

        int n_entries = root->inode.size / sizeof(struct wfs_dentry);
        struct wfs_dentry* dentry = (struct wfs_dentry*) root->data;
        for (int i = 0; i < n_entries; i++, dentry++)
            printf("Inode Number: %lu\tName: %s\n", dentry->inode_number, dentry->name);
        printf("\n");
    }

    if (remove) {
        if (strstr(argv[argc - 2], "prebuilt_disk") == NULL) {
            printf("-r requires disk_file to be \"prebuilt_disk\"\n");
            goto done;
        }

        struct wfs_log_entry* root = get_log_entry(0);

        if (!root) {
            WFS_ERROR("Failed to fetch root entry\n");
            goto done;
        }

        struct wfs_dentry dentry = {
            .name = "file1",
            .inode_number = 2,
        };

        if (remove_dentry(&root, &dentry)) {
            WFS_ERROR("Failed to remove dentry! {.name = %s, .inode_number = %lu}\n", dentry.name, dentry.inode_number);
        }


        int n_entries = root->inode.size / sizeof(struct wfs_dentry);
        struct wfs_dentry* dentry = (struct wfs_dentry*) root->data;
        for (int i = 0; i < n_entries; i++, dentry++)
            printf("Inode Number: %lu\tName: %s\n", dentry->inode_number, dentry->name);

        printf("\n");
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
