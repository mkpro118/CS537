#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#define FUSE_USE_VERSION 30
#include <fuse.h>


#define EITWNB -1 /* Error ITable Was Not Built */
#define EITWR  -2 /*   Error ITable Was Reset   */

static FILE* img_file = NULL;

/* Something like a cache for faster lookups from the disk image */
static struct {
    unsigned int* table;
    unsigned int  size;
    unsigned int  capacity;
} itable = {
    .table = NULL,
    .size = 0,
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
static void fll_i_table(FILE* img_file, unsigned int inode_number, off_t offset) {
    // If inode table is not yet present, we [re]started the FS
    // We detect whether or not inode table is present using the size
    // value in itable. If size = 0, then we have no information yet
    // Since we should have at least one inode for the root directory,
    // minimum value of size would be 1 for the FS.
}

static int icrease_i_table_capacity(unsigned int capacity) {
    int* temp;
    switch (itable.table) {
    case NULL:
        itable.capacity = capacity;
        temp = malloc(sizeof(int) * itable.capacity);
        if (!itable.table) {
            perror("FATAL ERROR: Failed to increase capacity (Malloc failed)!\n");
            perror("ABORTING Increase itable Capacity operation! Reset itable capacity and size to 0. ");
            perror("Future operations should try to re-read the disk image to re-build the itable.\n");
            return EITWNB;
        }
        break;
    default:
        itable.capacity += capacity;
        itable.table = realloc(sizeof(int) * itable.capacity);
        if (!itable.table) {
            perror("FATAL ERROR: Failed to increase capacity (Realloc failed)!\n");
            perror("ABORTING Increase Itable Capacity operation! Restoring itable capacity and size.");
            perror("Orignal data should be intact.\n");
            return EITWR;
        }
        break;
    }
    return 0;
}

static int read_image_file(file) {

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
    filler(buf, ".", NULL, 0); // Current directory
    filler(buf, "..", NULL, 0); // Previous directory
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

    img_file = argv[argc - 2];
    argv[argc - 2] = argv[--argc];
    argv[argc] = NULL;
    for (int i = 0; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");
    return 0;
    // return fuse_main(argc, argv, &ops, NULL);
}
