#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

// #define FUSE_USE_VERSION 30
// #include <fuse.h>

#ifndef FUSE_USE_VERSION
struct fuse_file_info {};
enum fuse_readdir_flags {
        FUSE_READDIR_PLUS = (1 << 0)
};

enum fuse_fill_dir_flags {
        FUSE_FILL_DIR_PLUS = (1 << 1)
};
typedef int(* fuse_fill_dir_t) (void *buf, const char *name, const struct stat *stbuf, off_t off, enum fuse_fill_dir_flags flags);
struct fuse_operations {
    int (*getattr)(const char* path, struct stat* stbuf);
    int (*mknod)(const char* path, mode_t mode, dev_t rdev);
    int (*mkdir)(const char* path, mode_t mode);
    int (*read)(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
    int (*write)(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
    int (*readdir)(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi);
};
#endif


#define ITOPSC  0 /* I-Table operation succeeded */
#define EITWNB -1 /* Error I-Table Was Not Built */
#define EITWR  -2 /*   Error I-Table Was Reset   */
#define EITWNR -3 /* Error I-Table Was Not Reset */

static FILE* img_file = NULL;

/* Something like a cache for faster lookups from the disk image */
static struct {
    unsigned int* table;
    unsigned int  capacity;
} itable = {
    .table    = NULL,
    .capacity = 0,
};

static const unsigned int ITABLE_CAPACITY_INCREMENT = 10;

/**
 * @todo : Implement
 *
 * Adds or updates itable entries for the given inode
 *
 * @param img_file     [description]
 * @param inode_number [description]
 * @param offset       [description]
 */
static void fill_itable(FILE* img_file, unsigned int inode_number, off_t offset) {
    // If inode table is not yet present, we [re]started the FS
    // We detect whether or not inode table is present using the size
    // value in itable. If size = 0, then we have no information yet
    // Since we should have at least one inode for the root directory,
    // minimum value of size would be 1 for the FS.
}

/**
 * Increase the capacity of the I-Table
 * @param  capacity [description]
 * @return          [description]
 */
static int icrease_itable_capacity(unsigned int capacity) {
    int* temp;
    switch (itable.capacity) {
    case 0:
        if (itable.table) {
            free(itable.table);
        }
        itable.table = NULL;
        itable.capacity = capacity;
        temp = malloc(sizeof(int) * itable.capacity);
        if (!temp) {
            perror("FATAL ERROR: Failed to increase capacity (Malloc failed)!\n");
            perror("ABORTING Increase itable Capacity operation! Reset itable capacity and size to 0.");

            itable.capacity = 0;

            perror("Future operations should try to re-read the disk image to re-build the itable.\n");
            return EITWNB;
        }
        break;
    default:
        itable.capacity += capacity;
        temp = realloc(itable.table, sizeof(int) * itable.capacity);
        if (!temp) {
            perror("FATAL ERROR: Failed to increase capacity (Realloc failed)!\n");
            perror("ABORTING Increase Itable Capacity operation!\n");

            if (itable.table) {
                perror("Restoring itable capacity and size.");
                itable.capacity -= capacity;
                perror("Orignal data should be intact.\n");
                return EITWR;
            } else {
                perror("Failed to restore old data");
                return EITWNR;
            }
        }
        break;
    }
    return ITOPSC;
}

static int read_image_file(FILE* file) {
    fseek(file, 0, SEEK_SET);

    return 0;
}

static int wfs_getattr(const char* path, struct stat* stbuf) {
    return 0;
}

static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
    return 0;
}

static int wfs_mkdir(const char* path, mode_t mode) {
    return 0;
}

static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return 0;
}

static int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return 0;
}

static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    filler(buf,  ".", NULL, 0, 0); // Current  directory
    filler(buf, "..", NULL, 0, 0); // Previous directory
    return 0;
}

static int wfs_unlink(const char* path) {
    return 0;
}


static struct fuse_operations ops = {
    .getattr = wfs_getattr,
    .mknod   = wfs_mknod,
    .mkdir   = wfs_mkdir,
    .read    = wfs_read,
    .write   = wfs_write,
    .readdir = wfs_readdir,
    .unlink  = wfs_unlink,
};

int main(int argc, const char *argv[]) {
    if (argc < 3) {
        printf("Usage: $ mount.wfs [FUSE options] disk_path mount_point\n");
        return 0;
    }

    img_file = fopen(argv[argc - 2], "ab+");

    int fuse_argc = argc - 1;

    img_file = argv[argc - 2];
    argv[argc - 2] = argv[fuse_argc];
    argv[fuse_argc] = NULL;

    return fuse_main(fuse_argc, argv, &ops, NULL);
}
