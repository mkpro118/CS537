#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "wfs.h"

#ifdef __unix__
#define FUSE_USE_VERSION 30
#include <fuse.h>
#else
struct fuse_file_info {};
enum fuse_readdir_flags {
        FUSE_READDIR_PLUS = (1 << 0)
};

enum fuse_fill_dir_flags {
        FUSE_FILL_DIR_PLUS = (1 << 1)
};
typedef int(* fuse_fill_dir_t) (void *buf, const char *name, const struct stat *stbuf, off_t off);
struct fuse_operations {
    int (*getattr)(const char* path, struct stat* stbuf);
    int (*mknod)(const char* path, mode_t mode, dev_t rdev);
    int (*mkdir)(const char* path, mode_t mode);
    int (*read)(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
    int (*write)(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
    int (*readdir)(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi);
    int (*unlink)(const char* path);
};
#endif


#define ITOPSC  0 /* I-Table operation succeeded */
#define EITWNB -1 /* Error I-Table Was Not Built */
#define EITWR  -2 /*   Error I-Table Was Reset   */
#define EITWNR -3 /* Error I-Table Was Not Reset */

static FILE* restrict disk_file = NULL;

/* Something like a cache for faster lookups from the disk image */
static struct {
    unsigned int* restrict table;
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
 * @param disk_file     [description]
 * @param inode_number [description]
 * @param offset       [description]
 */
/*
static void fill_itable(FILE* disk_file, unsigned int inode_number, off_t offset) {
    // If inode table is not yet present, we [re]started the FS
    // We detect whether or not inode table is present using the size
    // value in itable. If size = 0, then we have no information yet
    // Since we should have at least one inode for the root directory,
    // minimum value of size would be 1 for the FS.
}*/

/**
 * Increase the capacity of the I-Table
 *
 * @param capacity The new capacity of the I-Table
 *
 * @return Returns 0 on success, or on failure returns
 *           - EITWNB  I-Table Was Not Built
 *           - EITWR   I-Table Was Reset
 *           - EITWNR  I-Table Was Not Reset
 */
static int set_itable_capacity(unsigned int capacity) {
    printf("Setting Itable Capacity to %d\n", capacity);
    unsigned int* temp;
    printf("Current itable.capacity = %i\n", itable.capacity);
    switch (itable.capacity) {
    case 0:
        if (itable.table)
            free(itable.table);

        itable.table = NULL;
        itable.capacity = capacity;

        temp = malloc(sizeof(int) * itable.capacity);

        if (!temp) {
            itable.capacity = 0;
            return EITWNB;
        }
        break;
    default:
        temp = realloc(itable.table, sizeof(unsigned int) * capacity);

        if (!temp) {
            if (itable.table)
                return EITWR;
            else
                return EITWNR;
        }

        itable.capacity = capacity;
    }

    itable.table = temp;

    return ITOPSC;
}

static int init_itable() {
    fseek(disk_file, sizeof(struct wfs_sb), SEEK_SET);

    int seek = ftell(disk_file);

    while (seek < sb.head) {
        struct wfs_log_entry* entry = malloc(sizeof(struct wfs_log_entry));
        if (!entry) {
            perror("MALLOC FAILED!\n");
            return -1;
        }


        if (fread(entry, sizeof(struct wfs_inode), 1, disk_file) != 1) {
            perror("fread Failed!\n");
            return -1;
        }


        int data_size = entry->inode.size;
        int inode_number = entry->inode.inode_number;

        if (inode_number >= itable.capacity) {
            int err = set_itable_capacity(inode_number + ITABLE_CAPACITY_INCREMENT);

            switch (err) {
            case ITOPSC:
                goto success;
                break;
            case EITWNB:
                perror("FATAL ERROR: Failed to increase capacity (Malloc failed)!\n");
                perror("ABORTING Increase itable Capacity operation! Reset itable capacity and size to 0.\n");
                perror("Future operations should try to re-read the disk image to re-build the itable.\n");
                break;
            case EITWR:
                perror("Orignal data should be intact.\n");
                break;
            case EITWNR:
                perror("Failed to restore old data.\n");
                break;
            }
            return 1;
        }
        success:
        printf("itable.capacity = %i | inode_number = %i\n", itable.capacity, inode_number);
        if (!entry->inode.deleted) {
            itable.table[inode_number] = ftell(disk_file) - sizeof(struct wfs_inode);
        } else {
            itable.table[inode_number] = 0;
            printf("Skipping inode because it is deleted");
        }

        fseek(disk_file, data_size, SEEK_CUR);
        seek = ftell(disk_file);
    }

    return 0;
}

static void validate_wfs() {
    fseek(disk_file, 0, SEEK_SET);
    struct wfs_sb sb;

    if(fread(&sb, sizeof(struct wfs_sb), 1, disk_file) != 1) {
        perror("fread failed!\n");
        exit(1);
    }

    if (sb.magic != WFS_MAGIC) {
        perror("FATAL ERROR: File is not a WFS FileSystem disk image.\n");
        exit(1);
    }
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
    filler(buf,  ".", NULL, 0); // Current  directory
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

    printf("disk_path = %s\n", argv[argc - 2]);
    disk_file = fopen(argv[argc - 2], "a+");

    if (!disk_file) {
        perror("FRICK FILE WASN'T OPENED!!!!\n");
    }

    validate_wfs();
    set_itable_capacity(1);

    printf("Building I-Table...\n");
    int i = init_itable(disk_file);
    printf("Built I-Table | Status: %d\n", i);

    /* For testing */
    for (unsigned int i = 0; i < itable.capacity; i++) {
        printf("%d | ", itable.table[i]);
    }

    printf("\n");

    int fuse_argc = argc - 1;

    argv[argc - 2] = argv[fuse_argc];
    argv[fuse_argc] = NULL;

    printf("%p\n", (void*)&ops);
	
    return 0;
    // return fuse_main(fuse_argc, argv, &ops, NULL);
}
