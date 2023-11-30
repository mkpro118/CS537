#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "wfs.h"

//////////////// MK COPING WITH THE LINTER, NEVERMIND THIS BLOCK ///////////////

#ifdef __unix__
#define FUSE_USE_VERSION 30
#include <fuse.h>
#else
char* strtok_r(char*, char*, char**);
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

int fuse_main(int argc, char* argv[], struct fuse_operations* ops, void* opts);

#endif
////////////////////////////// COPING BLOCK ENDS ///////////////////////////////


////////////////////////////////// ERROR CODES /////////////////////////////////

#define ITOPSC  0 // I-Table operation succeeded        | Success Code
#define ITOPFL -1 // I-Table operation failed (generic) | Failure Code
#define EITWNB -2 // I-Table Was Not Built              |  Error  Code
#define EITWR  -3 // I-Table Was Reset                  |  Error  Code
#define EITWNR -4 // I-Table Was Not Reset              |  Error  Code


#define FSOPSC 0  // File System operation succeeded    | Success Code
#define FSOPFL 1  // File System operation failed       | Failure Code
////////////////////////////////// ERROR CODES /////////////////////////////////


/////////////////////////// FUNCTION PROTOTYPES START //////////////////////////

static void _check();
static int build_itable();
static void fill_itable(unsigned int inode_number, long offset);
static inline void invalidate_itable();
static int set_itable_capacity(unsigned int capacity);
static inline void invalidate_path_history();
static int parse_path(const char* restrict path, unsigned int* restrict out);
static struct wfs_log_entry* get_log_entry(unsigned int inode_number);
//////////////////////////// FUNCTION PROTOTYPES END ///////////////////////////


//////////////////////////// BOOKKEEPING VARIABLES /////////////////////////////

#define ITABLE_CAPACITY_INCREMENT 10
#define PATH_HISTORY_CAPACITY_INCREMENT 5
#define MAX_PATH_LENGTH 128

/**
 * In memory pseudo-superblock. (ps_sb = PSeudo SuperBlock)
 * Holds some information about the WFS in memory
 * This should be validated using the _check() function before any operation
 */
static struct {
    unsigned char is_valid;
    unsigned int n_inodes;
    unsigned int n_log_entries;
    char* restrict disk_filename;
    FILE* restrict disk_file;
    struct {
        long* restrict table;   // This struct is like a cache for
        unsigned int  capacity; // faster lookups from the disk image.
    } itable;                   // This is set in build_itable().
    struct {
        unsigned int* history;  // This records the path taken, to traverse
        unsigned int capacity;  // backwards for relative paths.
    } path_history;             // Used only by parse_path
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
    .path_history = {
        .history  = NULL,
        .capacity = 0,
    },
    .sb = {
        .magic = 0,
        .head = 0,
    },
};

////////////////////////// BOOKKEEPING VARIABLES END ///////////////////////////


////////////////////// I-TABLE MANAGEMENT FUNCTIONS START //////////////////////

/**
 * Performs checks to verify in-memory data structures are intact
 */
static void _check() {
	WFS_DEBUG("PERFORM CHECKS\n");
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
}


/**
 * Adds or updates itable entries for the given inode
 *
 * @param inode_number Inode this entry is for
 * @param offset       The offset of the most recent entry for the given inode
 */
static void fill_itable(unsigned int inode_number, long offset) {
    _check();

    if (ps_sb.itable.capacity <= inode_number)
        set_itable_capacity(inode_number + ITABLE_CAPACITY_INCREMENT);

    ps_sb.itable.table[inode_number] = offset;
}


/**
 * Invalidates the I-Table
 */
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
 * @return Returns ITOPSC on success, or on failure returns
 *           - EITWNB  I-Table Was Not Built
 *           - EITWR   I-Table Was Reset
 *           - EITWNR  I-Table Was Not Reset
 */
static int set_itable_capacity(unsigned int capacity) {
    _check();

    WFS_DEBUG("Setting Itable Capacity to %d\n", capacity);
    off_t* temp;
    WFS_DEBUG("Current ps_sb.itable.capacity = %i\n", ps_sb.itable.capacity);
    switch (ps_sb.itable.capacity) {
    case 0:
        if (ps_sb.itable.table)
            free(ps_sb.itable.table);

        ps_sb.itable.table = NULL;
        ps_sb.itable.capacity = capacity;

        temp = malloc(sizeof(off_t) * ps_sb.itable.capacity);

        if (!temp) {
            ps_sb.itable.capacity = 0;
            return EITWNB;
        }
        break;
    default:
        temp = realloc(ps_sb.itable.table, sizeof(off_t) * capacity);

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

/**
 * Reads the disk image file to build the I-Table
 *
 * @return ITOPSC on success, on failure returns
 *           - EITWNB  I-Table Was Not Built
 *           - EITWR   I-Table Was Reset
 *           - EITWNR  I-Table Was Not Reset
 */
static int build_itable() {
    ps_sb.n_inodes = 0;
    ps_sb.n_log_entries = 0;

    fseek(ps_sb.disk_file, sizeof(struct wfs_sb), SEEK_SET);

    long seek;

    while ((seek = ftell(ps_sb.disk_file)) < ps_sb.sb.head) {
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

        ps_sb.n_log_entries++;

        int data_size = entry->inode.size;
        int inode_number = entry->inode.inode_number;

        ps_sb.n_inodes = inode_number >= ps_sb.n_inodes ? inode_number + 1 : ps_sb.n_inodes;

        if (inode_number >= ps_sb.itable.capacity) {
            int err = set_itable_capacity(inode_number + ITABLE_CAPACITY_INCREMENT);

            switch (err) {
            case EITWNB:
                WFS_ERROR("Failed to increase capacity (Malloc failed)!\n");
                WFS_ERROR("ABORTING Increase itable Capacity operation! Reset itable capacity and size to 0.\n");
                WFS_ERROR("Future operations should try to re-read the disk image to re-build the ps_sb.itable.\n");
                invalidate_itable();
                return EITWNB;
            case EITWR:
                WFS_ERROR("Orignal data should be intact.\n");
                return EITWR;
            case EITWNR:
                WFS_ERROR("Failed to restore old data.\n");
                invalidate_itable();
                return EITWNR;
            }
        }

        fill_itable(inode_number, entry->inode.deleted ? 0l : seek);

        fseek(ps_sb.disk_file, data_size, SEEK_CUR);
        free(entry);
    }

    return ITOPSC;
}

/////////////////////// I-TABLE MANAGEMENT FUNCTIONS END ///////////////////////


//////////////////// FILE SYSTEM MANAGEMENT FUNCTIONS START ////////////////////

static inline void invalidate_path_history() {
    int capacity;
    if (!ps_sb.path_history.history) {
        capacity = PATH_HISTORY_CAPACITY_INCREMENT;
        ps_sb.path_history.history = malloc(sizeof(unsigned int) * capacity);
        if (!ps_sb.path_history.history) {
            WFS_ERROR("Malloc Failed!\n");
            exit(FSOPFL);
        }
    } else {
        capacity = ps_sb.path_history.capacity;
    }

    memset(ps_sb.path_history.history, (unsigned int) -1, capacity);
    *ps_sb.path_history.history = 0; // Set the root to be inode 0
    ps_sb.path_history.capacity = capacity;
}

/**
 * Increase the capacity of the I-Table
 *
 * @param capacity The new capacity of the I-Table
 *
 * @return Returns FSOPSC on success, FSOPFL on failure returns
 */
static int set_path_history_capacity(unsigned int capacity) {
    _check();

    unsigned int* temp;
    switch (ps_sb.path_history.capacity) {
    case 0:
        invalidate_path_history();
        return ITOPSC;
    default:
        temp = realloc(ps_sb.path_history.history, sizeof(unsigned int) * capacity);

        if (!temp) {
            WFS_ERROR("Realloc failed!\n");
            return FSOPFL;
        }

        ps_sb.path_history.capacity = capacity;
    }

    ps_sb.path_history.history = temp;

    return ITOPSC;
}

/**
 * Given a path, this function will parse it to find the inode number
 * corresponding to the file or directory point to by this
 * @param  path The path to the file (shouldn't be > 128 characters)
 * @param  out  The address of the out variable
 *
 * @return FSOPSC on success, FSOPFL on failure
 */
static int parse_path(const char* path, unsigned int* out) {
    invalidate_path_history();
    char* _path = NULL;

    // All paths should start with a "/"
    if (*path != '/')
        goto fail;

    // If the filepath is literally the root directory, return inode 0
    if (strcmp("/", path) == 0) {
        *out = 0;
        goto success;
    }

    // Strip any leading forward slashes
    const char* p = path;
    while (*p++ == '/');

    _path = strdup(p);
    ssize_t len = strlen(_path);

    // Strip any trailing forward slashes
    while (_path[len - 1] == '/')
        _path[--len] = '\0';

    char* base_filename = strrchr(_path, '/');

    // Special Case, File is in the root directory.
    if (!base_filename) {
        base_filename = _path;
        struct wfs_log_entry* entry = get_log_entry(0);

        // Handle Dentry

    }


    char* token;
    char* context;

    int ph_idx = 1;
    while ((token = strtok_r(_path, "/", &context)) != NULL) {
        if (strcmp(".", token) == 0) {
            continue;
        } else if (strcmp("..", token) == 0) {
            ph_idx -= ph_idx != 1;
        }
        struct wfs_log_entry* entry = get_log_entry(ps_sb.path_history.history[ph_idx - 1]);

        if
    }


    success:
    free(_path);
    return FSOPSC;

    fail:
    if (_path)
        free(_path);

    WFS_ERROR("Path %s is not a valid path!\n", path);
    return FSOPFL;
}


static struct wfs_log_entry* get_log_entry(unsigned int inode_number) {
    return NULL;
}

//////////////////// FILE SYSTEM MANAGEMENT FUNCTIONS START ////////////////////


///////////////////// DISK FILE MANAGEMENT FUNCTIONS START /////////////////////

/**
 * Parses the superblock of the given disk file to ensure the file is a valid
 * WFS disk image
 *
 * Exits with a failure code if the given file is invalid
 */
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

////////////////////// DISK FILE MANAGEMENT FUNCTIONS END //////////////////////


///////////////////////// FUSE HANDLER FUNCTIONS START /////////////////////////

static int wfs_getattr(const char* path, struct stat* stbuf) {
    _check();
    unsigned int inode_number;

    if(parse_path(path, &inode_number) != FSOPSC) {
        WFS_ERROR("Failed to find inode\n");
    }

    struct wfs_log_entry* original = get_log_entry(inode_number);
    struct wfs_inode tocopy = original->inode;
    stbuf->st_ino = tocopy.inode_number; 
    stbuf->st_mode = tocopy.mode;
    stbuf->st_nlink = tocopy.links;
    stbuf->st_uid = tocopy.uid;
    stbuf->st_gid = tocopy.gid;
    stbuf->st_size = tocopy.size;
    stbuf->st_atime = tocopy.atime; 
    stbuf->st_mtime = tocopy.mtime;
    stbuf->st_ctime = tocopy.ctime; 
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
        WFS_ERROR("strdup failed :(");
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

    WFS_INFO("Building I-Table...\n");

    int err;
    if((err = build_itable(ps_sb.disk_file)) != ITOPSC) {
        WFS_ERROR("Failed to build I-Table | Error Code: %d\n", err);

    }

    WFS_INFO("Built I-Table successfully!\n");
    WFS_INFO("Parsed %d log entries, with %d inodes\n", ps_sb.n_log_entries, ps_sb.n_inodes);

    WFS_DEBUG("%p\n", (void*)&ops);

    #if WFS_DBUG == 1

    /* For testing */
    for (unsigned int i = 0; i < ps_sb.itable.capacity; i++) {
        printf("%d | ", ps_sb.itable.table[i]);
    }

    printf("\n");

    return 0;
    #else
    int fuse_argc = argc - 1;

    argv[argc - 2] = argv[fuse_argc];
    argv[fuse_argc] = NULL;

    WFS_DEBUG("%p\n", (void*)&ops);
    return fuse_main(fuse_argc, argv, &ops, NULL);
    #endif
}
