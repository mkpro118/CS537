#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
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

#define SERVER_LOG(...) do {\
    fprintf(stdout, "\n\x1b[33m:LOG:\x1b[0m (%s) ", __func__);\
    fprintf(stdout, __VA_ARGS__);\
} while(0)



static int wfs_getattr(const char* path, struct stat* stbuf) {
    SERVER_LOG("path: %s\n", path);
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

    if (entry->inode.deleted) {
        free(entry);
        return -ENOENT;
    }

    wfs_stat_init(stbuf, &entry->inode);

    free(entry);
    return 0;
}

static int make_inode(const char* path, mode_t mode) {
    SERVER_LOG("path: %s\tmode: %x\n", path, mode);
    if (strlen(path) == 0) {
        WFS_ERROR("Cannot create file with empty name\n");
        return -1;
    }

    if (strcmp(path, "/") == 0) {
        WFS_ERROR("Cannot create root\n");
        return -1;
    }

    char* _path = simplify_path(path);
    SERVER_LOG("simplified path: %s\n", _path);
    char* base_file = strrchr(_path, '/');

    uint parent_inode = 0;
    if (base_file) {
        *base_file = 0;
        base_file++;
        SERVER_LOG("Parent path: %s\n", _path);
        if (parse_path(_path, &parent_inode) != FSOPSC) {
            WFS_ERROR("Failed to find parent directory %sn", _path);
            free(_path);
            return -ENOENT;
        }
    } else {
        base_file = _path;
    }

    ssize_t len = strlen(base_file);

    SERVER_LOG("BASE_FILE: '%s' | strlen = %lu\n", base_file, len);

    if (len >= MAX_FILE_NAME_LEN) {
        WFS_ERROR("File name %s is too long\n", base_file);
        free(_path);
        return -1;
    }

    SERVER_LOG("PARENT INODE: %d\n", parent_inode);
    struct wfs_log_entry* parent = get_log_entry(parent_inode);

    if (!parent) {
        WFS_ERROR("Failed to find log_entry for inode %d.\n", parent_inode);
        free(_path);
        return -ENOENT;
    }

    uint out = -1;

    if (find_file_in_dir(parent, base_file, &out) == FSOPSC) {
        struct wfs_log_entry* entry = get_log_entry(out);
        if (!entry->inode.deleted) {
            WFS_ERROR("File %s already exists!\n", base_file);
            free(_path);
            free(parent);
            free(entry);
            return -EEXIST;
        }
        free(entry);
    }

    struct wfs_log_entry child;

    wfs_inode_init(&child.inode, mode);

    struct wfs_dentry dentry = {
        .name = {0},
        .inode_number = child.inode.inode_number,
    };

    strncpy(dentry.name, base_file, len);
    SERVER_LOG("INTIAL PARENT SIZE = %d\n", parent->inode.size);
    if (add_dentry(&parent, &dentry)) {
        WFS_ERROR("Failed to add dentry! {.name = \"%s\", .inode_number = %lu}\n",
                  dentry.name, dentry.inode_number);
        free(parent);
        free(_path);
        ps_sb.n_inodes--;
        return -1;
    }
    SERVER_LOG("NEW PARENT SIZE = %d\n", parent->inode.size);

    begin_op();
    int retval = append_log_entry(parent);
    if (retval != -ENOSPC)
        retval = append_log_entry(&child);
    end_op();

    SERVER_LOG("wth we wrote to file!\n");
    free(_path);
    free(parent);
    return retval == -ENOSPC ? retval: 0;
}

static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
    SERVER_LOG("path: %s\tmode: %x\n", path, mode);
    _check();

    return make_inode(path, FILE_MODE | mode);
}

static int wfs_mkdir(const char* path, mode_t mode) {
    SERVER_LOG("path: %s\tmode: %x\n", path, mode);
    _check();

    return make_inode(path, DIRECTORY_MODE | mode);
}

static int wfs_read(const char* path, char* buf, size_t size,
                    off_t offset, struct fuse_file_info* fi) {
    SERVER_LOG("path: %s\tsize: %lu\toffset: %lu\n", path, size, offset);
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

    if (_check_reg_inode(&entry->inode)) {
        free(entry);
        return -EACCES;
    }

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

    memcpy(buf, data + offset, n_bytes);

    done:
    free(entry);
    return n_bytes;
}

static int wfs_write(const char* path, const char* buf, size_t size,
                     off_t offset, struct fuse_file_info* fi) {
    SERVER_LOG("path: %s\tsize: %li\toffset: %lu\n", path, size, offset);
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

    if (_check_reg_inode(&entry->inode)) {
        WFS_ERROR("Can only write to regular files. Found mode %x.", entry->inode.mode);
        return -1;
    }

    off_t result_offset = offset + size;

    if(result_offset > entry->inode.size){
        size_t new_size = sizeof(struct wfs_log_entry) + result_offset;

        struct wfs_log_entry* temp = realloc(entry, new_size);

        if (!temp) {
            WFS_ERROR("realloc failed!");
            return FSOPFL;
        }

        entry = temp;
        entry->inode.size = result_offset;
    }

    memcpy(entry->data + offset, buf, size);

    begin_op();
    int retval = append_log_entry(entry);
    end_op();

    free(entry);
    return retval == -ENOSPC ? retval : size;
}

static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info* fi) {
     SERVER_LOG("path: %s\toffset: %lu\n", path, offset);
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

    SERVER_LOG("n_entries = %d\n", n_entries);

    struct wfs_dentry* dentry = (struct wfs_dentry*) entry->data;

    for (int i = 0; i < n_entries; i++, dentry++) {
        SERVER_LOG("  -> %s\n", dentry->name);
        inode_number = dentry->inode_number;
        struct wfs_log_entry* entry = get_log_entry(inode_number);

        if (!entry) {
            WFS_ERROR("Failed to find log_entry for inode %u.\n", inode_number);
            continue;
        }

        if (entry->inode.deleted){
            free(entry);
            continue;
        }

        struct stat stbuf;
        wfs_stat_init(&stbuf, &entry->inode);

        filler(buf, dentry->name, &stbuf, 0);
        free(entry);
    }

    free(entry);
    return 0;
}

static int wfs_unlink(const char* path) {
    SERVER_LOG("path: %s\n", path);
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

    off_t offset = lookup_itable(inode_number);

    begin_op();
    write_to_disk(offset, entry);
    end_op();
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
        if (!strncpy(dentry.name, _path, MAX_FILE_NAME_LEN)) {
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

        begin_op();
        int retval = append_log_entry(entry);
        end_op();

        free(entry);
        free(_path);
        return retval;
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

    begin_op();
    int retval = append_log_entry(entry);
    end_op();

    free(entry);
    free(_path);
    return retval;
}

#if WFS_EXE == 1
int wfs_chmod(const char* path, mode_t mode) {
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

    entry->inode.mode &= ~0777;
    entry->inode.mode |= (mode & 0777);

    off_t offset = lookup_itable(inode_number);

    begin_op();
    write_to_disk(offset, entry);
    end_op();
    free(entry);
    return 0;
}
#endif

////////////////////////// FUSE HANDLER FUNCTIONS END //////////////////////////


////////////////////// FILE CHANGE SIGNAL HANDLERS START ///////////////////////

#ifdef WFS_MMAP

void sigusr2_handler(int signum) {
    invalidate_itable();
    build_itable();
}

#endif

void sigusr1_handler(int signum) {
    if (ps_sb.wfs)
        return;

    #ifdef WFS_MMAP
    wfs_file* f = wfs_freopen(ps_sb.disk_filename, "r+", ps_sb.disk_file);
    #else
    FILE* f = wfs_freopen(ps_sb.disk_filename, "r+", ps_sb.disk_file);
    #endif

    if (!f) {
        WFS_ERROR("wfs_freopen failed!\n");
        exit(FSOPFL);
    }

    ps_sb.disk_file = f;
    setup_flocks();
}

/////////////////////// FILE CHANGE SIGNAL HANDLERS END ////////////////////////


/**
 * Register callbacks for fuse operations
 */
static struct fuse_operations ops = {
    .getattr = wfs_getattr,
    .mknod   = wfs_mknod,
    .mkdir   = wfs_mkdir,
    .read    = wfs_read,
    .write   = wfs_write,
    .readdir = wfs_readdir,
    .unlink  = wfs_unlink,
#if WFS_EXE == 1
    .chmod   = wfs_chmod,
#endif
};

/**
 * Mounts the WFS on the mount point using the disk file using FUSE
 *
 * Usage:
 *    $: mount.wfs [FUSE options] <disk_path> <mount_point>
 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: $ mount.wfs [FUSE options] disk_path mount_point\n");
        return 0;
    }

    wfs_init(argv[0], argv[argc - 2]);

    WFS_DEBUG("%p\n\n", (void*)&ops);

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

        WFS_INFO("Adding dentry {.name = \"%s\", .inode_number = %lu}\n",
                 dentry.name, dentry.inode_number);

        if (add_dentry(&root, &dentry)) {
            WFS_ERROR("Failed to add dentry! {.name = \"%s\", .inode_number = %lu}\n",
                      dentry.name, dentry.inode_number);
        }

        ps_sb.n_inodes++;

        int n_entries = root->inode.size / sizeof(struct wfs_dentry);

        struct wfs_dentry* d = (struct wfs_dentry*) root->data;
        for (int i = 0; i < n_entries; i++, d++)
            printf("Inode Number: %lu\tName: %s\n", d->inode_number, d->name);
        printf("\n");

        if (root)
            free(root);
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

        WFS_INFO("Removing dentry {.name = \"%s\", .inode_number = %lu}\n", dentry.name, dentry.inode_number);

        if (remove_dentry(&root, &dentry)) {
            WFS_ERROR("Failed to remove dentry! {.name = \"%s\", .inode_number = %lu}\n", dentry.name, dentry.inode_number);
        }


        int n_entries = root->inode.size / sizeof(struct wfs_dentry);
        struct wfs_dentry* d = (struct wfs_dentry*) root->data;
        for (int i = 0; i < n_entries; i++, d++)
            printf("Inode Number: %lu\tName: %s\n", d->inode_number, d->name);

        printf("\n");

        if (root)
            free(root);
    }

    done:
    return 0;
    #else
    int fuse_argc = argc - 1;

    argv[argc - 2] = argv[fuse_argc];
    argv[fuse_argc] = NULL;

    // set up SIGUSR1
    {
        /* Open the directory to be monitored */
        char* path = simplify_path(ps_sb.disk_filename);
        char* base = strrchr(path, '/');
        if (base) {
            *base = 0;
            base = strdup(path);
        } else {
            base = strdup(".");
        }
        free(path);

        int fd = open(base, O_RDONLY);
        free(base);

        if (fd == -1) {
            WFS_ERROR("open failed!\n");
            return FSOPFL;
        }
        fcntl(fd, F_SETSIG, SIGUSR1);
        if (fcntl(fd, F_NOTIFY, DN_MODIFY | DN_MULTISHOT) == -1) {
            WFS_ERROR("fcntl for monitoring failed!\n");
            return FSOPFL;
        }

        struct sigaction sa;

        memset(&sa, 0, sizeof(struct sigaction));

        sa.sa_handler = sigusr1_handler;

        // ensure the handler is bound properly
        if (sigaction(SIGUSR1, &sa, NULL) < 0) {
            WFS_ERROR("FATAL ERROR: Couldn't bind SIGUSR1!\n");
            exit(FSOPFL);
        } else {
            WFS_INFO("Successfully setup SIGUSR1!\n");
        }
    }

    return fuse_main(fuse_argc, argv, &ops, NULL);
    #endif
}
