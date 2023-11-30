#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wfs.h"

//////////////// MK COPING WITH THE LINTER, NEVERMIND THIS BLOCK ///////////////

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
////////////////////////////// COPING BLOCK ENDS ///////////////////////////////


////////////////////////////////// ERROR CODES /////////////////////////////////

#define ITOPSC  0 // I-Table operation succeeded        | Success Code
#define ITOPFL -1 // I-Table operation failed (generic) | Failure Code
#define EITWNB -2 // I-Table Was Not Built              |  Error  Code
#define EITWR  -3 // I-Table Was Reset                  |  Error  Code
#define EITWNR -4 // I-Table Was Not Reset              |  Error  Code

////////////////////////////////// ERROR CODES /////////////////////////////////


/////////////////////////// FUNCTION PROTOTYPES START //////////////////////////

static void _check();
static int build_itable();
//static void fill_itable(unsigned int inode_number, off_t offset);
static inline void invalidate_itable();
static int set_itable_capacity(unsigned int capacity);

//////////////////////////// FUNCTION PROTOTYPES END ///////////////////////////


//////////////////////////// BOOKKEEPING VARIABLES /////////////////////////////

/**
 * In memory pseudo-superblock. (ps_sb = PSeudo SuperBlock)
 * Holds some bookkeeping information about WFS
 * This should be validated using the _check() function before any operation
 *
 */
static struct {
    unsigned char is_valid;
    unsigned int n_inodes;
    unsigned int n_log_entries;
    char* restrict disk_filename;
    FILE* restrict disk_file; // This is set in validate_disk_file().
    struct {
        unsigned int* restrict table; // This struct is like a cache for
        unsigned int  capacity;       // faster lookups from the disk image.
    } itable;                         // This is set in build_itable().

    struct wfs_sb sb; // This is set in validate_disk_file().
} ps_sb = {
    .is_valid = 0,
    .n_inodes = 0,
    .n_log_entries = 0,
    .disk_filename = NULL,
    .disk_file = NULL,
    .itable = {
        .table    = NULL,
        .capacity = 0,
    },
    .sb = {
        .magic = 0,
        .head = 0,
    },
};

////////////////////////// BOOKKEEPING VARIABLES END ///////////////////////////


////////////////////// I-TABLE MANAGEMENT FUNCTIONS START //////////////////////
static void _check() {
    if (!ps_sb.is_valid) {
        WFS_ERROR("Cannot perform operation because given disk_file is not a valid wfs disk_file");
        exit(ITOPFL);
    }

    if (ps_sb.sb.magic != WFS_MAGIC) {
        WFS_ERROR("File is not a WFS FileSystem disk image.\n");
        exit(ITOPFL);
    }

    if (ps_sb.sb.head < WFS_BASE_ENTRY_OFFSET) {
        WFS_ERROR("Invalid Superblock!\n");
        exit(ITOPFL);
    }

    if (!ps_sb.disk_filename) {
        WFS_ERROR("No disk file is specified\n");
    }

    if (!ps_sb.disk_file) {
        WFS_ERROR("No FILE handle for the %s was found", ps_sb.disk_filename);
        WFS_ERROR("Retrying once to re-build.\n");

        ps_sb.disk_file = fopen(ps_sb.disk_filename, "ab+");

        if (!ps_sb.disk_file || (build_itable() != ITOPSC)) {
            WFS_ERROR("Retry failed! Exiting!\n");
            exit(ITOPFL);
        }
    }

    if (ps_sb.n_inodes > ps_sb.itable.capacity) {
        set_itable_capacity(ps_sb.n_inodes);
    }
}

#define ITABLE_CAPACITY_INCREMENT 10

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
static void fill_itable(unsigned int inode_number, off_t offset) {
    // If inode table is not yet present, we [re]started the FS
    // We detect whether or not inode table is present using the size
    // value in ps_sb.itable. If size = 0, then we have no information yet
    // Since we should have at least one inode for the root directory,
    // minimum value of size would be 1 for the FS.
}*/

static inline void invalidate_itable() {
    if (ps_sb.itable.table)
        free(ps_sb.itable.table);

    ps_sb.itable.table = NULL;
    ps_sb.itable.capacity = 0;
}

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
    _check();

    WFS_DEBUG("Setting Itable Capacity to %d\n", capacity);
    unsigned int* temp;
    WFS_DEBUG("Current ps_sb.itable.capacity = %i\n", ps_sb.itable.capacity);
    switch (ps_sb.itable.capacity) {
    case 0:
        if (ps_sb.itable.table)
            free(ps_sb.itable.table);

        ps_sb.itable.table = NULL;
        ps_sb.itable.capacity = capacity;

        temp = malloc(sizeof(int) * ps_sb.itable.capacity);

        if (!temp) {
            ps_sb.itable.capacity = 0;
            return EITWNB;
        }
        break;
    default:
        temp = realloc(ps_sb.itable.table, sizeof(unsigned int) * capacity);

        if (!temp) {
            if (ps_sb.itable.table)
                return EITWR;
            else
                return EITWNR;
        }

        ps_sb.itable.capacity = capacity;
    }

    ps_sb.itable.table = temp;

    return ITOPSC;
}

static int build_itable() {
    ps_sb.n_inodes = 0;
    ps_sb.n_log_entries = 0;
    fseek(ps_sb.disk_file, sizeof(struct wfs_sb), SEEK_SET);

    int seek = ftell(ps_sb.disk_file);

    while (seek < ps_sb.sb.head) {
        struct wfs_log_entry* entry = malloc(sizeof(struct wfs_log_entry));
        if (!entry) {
            WFS_ERROR("MALLOC FAILED!\n");
            return -1;
        }

        if (fread(entry, sizeof(struct wfs_inode), 1, ps_sb.disk_file) != 1) {
            WFS_ERROR("fread Failed!\n");
            free(entry);
            return -1;
        }


        int data_size = entry->inode.size;
        int inode_number = entry->inode.inode_number;

        if (inode_number >= ps_sb.itable.capacity) {
            int err = set_itable_capacity(inode_number + ITABLE_CAPACITY_INCREMENT);

            switch (err) {
            case EITWNB:
                WFS_ERROR("Failed to increase capacity (Malloc failed)!\n");
                WFS_ERROR("ABORTING Increase itable Capacity operation! Reset itable capacity and size to 0.\n");
                WFS_ERROR("Future operations should try to re-read the disk image to re-build the ps_sb.itable.\n");
                return EITWNB;
            case EITWR:
                WFS_ERROR("Orignal data should be intact.\n");
                return EITWR;
            case EITWNR:
                WFS_ERROR("Failed to restore old data.\n");
                return EITWNR;
            }
        }

        WFS_DEBUG("ps_sb.itable.capacity = %i | inode_number = %i\n", ps_sb.itable.capacity, inode_number);
        if (!entry->inode.deleted) {
            ps_sb.itable.table[inode_number] = ftell(ps_sb.disk_file) - sizeof(struct wfs_inode);
        } else {
            ps_sb.itable.table[inode_number] = 0;
            WFS_DEBUG("Skipping inode because it is deleted");
        }

        fseek(ps_sb.disk_file, data_size, SEEK_CUR);
        seek = ftell(ps_sb.disk_file);
        free(entry);
    }

    return ITOPSC;
}

/////////////////////// I-TABLE MANAGEMENT FUNCTIONS END ///////////////////////

static void validate_disk_file() {
    ps_sb.is_valid = 0;
    fseek(ps_sb.disk_file, 0, SEEK_SET);

    if(fread(&ps_sb.sb, sizeof(struct wfs_sb), 1, ps_sb.disk_file) != 1) {
        WFS_ERROR("fread failed!\n");
        exit(ITOPFL);
    }

    ps_sb.is_valid = 1;
    _check();
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

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: $ mount.wfs [FUSE options] disk_path mount_point\n");
        return 0;
    }

    WFS_INFO("disk_path = %s\n", argv[argc - 2]);
    ps_sb.disk_file = fopen(argv[argc - 2], "a+");

    if (!ps_sb.disk_file) {
        WFS_ERROR("Couldn't open file \"%s\"\n", argv[argc - 2]);
        exit(ITOPFL);
    }

    validate_disk_file();
    invalidate_itable();
    set_itable_capacity(1);

    WFS_INFO("Building I-Table...\n");
    int i = build_itable(ps_sb.disk_file);
    WFS_INFO("Built I-Table | Status: %d\n", i);

    /* For testing */
    for (unsigned int i = 0; i < ps_sb.itable.capacity; i++) {
        WFS_DEBUG("%d | ", ps_sb.itable.table[i]);
    }

    WFS_DEBUG("\n");

    int fuse_argc = argc - 1;

    argv[argc - 2] = argv[fuse_argc];
    argv[fuse_argc] = NULL;

    WFS_DEBUG("%p\n", (void*)&ops);
	
    #if WFS_DEBUG == 1
    return 0;
    #else
    return fuse_main(fuse_argc, argv, &ops, NULL);
    #endif
}
