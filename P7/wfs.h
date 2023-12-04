#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef MOUNT_WFS_H_
#define MOUNT_WFS_H_

#define MAX_FILE_NAME_LEN 32
#define MAX_DISK_FILE_SIZE 1048576
#define WFS_MAGIC 0xdeadbeef

#ifdef WFS_SETUP
#define WFS_INITIAL_INODE_NUMBER 0
#else
#define WFS_INITIAL_INODE_NUMBER 1
#endif

#define WFS_INODE_INTACT  0
#define WFS_INODE_DELETED 1

#define WFS_IGNORED_FIELD 0

#define WFS_N_HARD_LINKS 1

#if WFS_DBUG == 1

#define WFS_DEBUG(...) do {\
    fprintf(stdout, "\x1b[33m:DEBUG (%d):\x1b[0m  ", __LINE__);\
    fprintf(stdout, __VA_ARGS__);\
} while(0)

#define PRINT_INODE(x) do {\
    printf("\n{\n");\
    printf("  inode_number: %u\n", x->inode_number);\
    printf("  deleted: %s\n", x->deleted ? "Yes" : "No");\
    printf("  mode: %u\t", x->mode);\
    printf("  uid: %u\t", x->uid);\
    printf("  gid: %u\n", x->gid);\
    printf("  size: %u\n", x->size);\
    printf("  atime:  %u\t", x->atime);\
    printf("  mtime:  %u\t", x->mtime);\
    printf("  ctime:  %u\n", x->ctime);\
    printf("}\n");\
} while(0)

#define PRINT_LOG_ENTRY(x) do {\
    printf("\n------------------------------------------------------------\n");\
    PRINT_INODE((&(x)->inode));\
\
    if (S_ISREG(((x)->inode.mode))) {\
        char* buffer = calloc((x)->inode.size + 1, sizeof(char));\
        strncpy(buffer, (x)->data, (x)->inode.size);\
        buffer[(x)->inode.size] = '\0';\
        printf("File Content:\n%s\n", buffer);\
        free(buffer);\
    }\
    else if (S_ISDIR(((x)->inode.mode))) {\
        printf("Directory Contents:\n");\
\
        int n_entries = (x)->inode.size / sizeof(struct wfs_dentry);\
\
        struct wfs_dentry* dentry = (struct wfs_dentry*) (x)->data;\
\
        for (int i = 0; i < n_entries; i++, dentry++) {\
            struct wfs_log_entry* temp = get_log_entry(dentry->inode_number);\
            if (!temp) {\
                printf("NOOO WE FAILED!\n");\
                exit(1);\
            }\
            if (S_ISDIR((temp->inode.mode))) {\
                printf("Inode: %lu\tType: Dir\tName: %s\n", dentry->inode_number, dentry->name);\
            } else if (S_ISREG((temp->inode.mode))) {\
                printf("Inode: %lu\tType: File\tName: %s\n", dentry->inode_number, dentry->name);\
            } else {\
                printf("frick we failed again\n");\
            }\
            free(temp);\
        }\
    }\
    else printf("UNSUPPORTED TYPE\n");\
    printf("------------------------------------------------------------\n");\
} while(0)

#else

#define WFS_DEBUG(...) (void)0

#endif

#define WFS_INFO(...) do {\
    fprintf(stdout, "\x1b[32m:Info:\x1b[0m  ");\
    fprintf(stdout, __VA_ARGS__);\
} while(0)

#define WFS_ERROR(...) do {\
    fprintf(stderr, "\x1b[31m:ERROR:\x1b[0m (%s) ", __func__);\
    fprintf(stderr, __VA_ARGS__);\
} while(0)

#ifdef _WIN32
unsigned int getuid() { return 0; }
unsigned int getgid() { return 0; }
#endif

#define WFS_USER_ID   ((unsigned int) getuid())
#define WFS_GROUP_ID  ((unsigned int) getgid())
#define WFS_CURR_TIME ((unsigned int) time(NULL))

struct wfs_sb {
    uint32_t magic;
    uint32_t head;
};

struct wfs_inode {
    unsigned int inode_number;
    unsigned int deleted;       // 1 if deleted, 0 otherwise
    unsigned int mode;          // type. S_IFDIR if the inode represents a directory or S_IFREG if it's for a file
    unsigned int uid;           // user id
    unsigned int gid;           // group id
    unsigned int flags;         // flags
    unsigned int size;          // size in bytes
    unsigned int atime;         // last access time
    unsigned int mtime;         // last modify time
    unsigned int ctime;         // inode change time (the last time any field of inode is modified)
    unsigned int links;         // number of hard links to this file (this can always be set to 1)
};

struct wfs_dentry {
    char name[MAX_FILE_NAME_LEN];
    unsigned long inode_number;
};

struct wfs_log_entry {
    struct wfs_inode inode;
    char data[];
};

#define WFS_INIT_ROOT_OFFSET  (sizeof(struct wfs_sb))
#define WFS_BASE_ENTRY_OFFSET (sizeof(struct wfs_sb) + sizeof(struct wfs_log_entry))
#define WFS_LOG_ENTRY_SIZE(x) (sizeof(struct wfs_log_entry) + (sizeof(char) * (x)->inode.size))

enum InodeModes {
    FILE_MODE = S_IFREG,
    DIRECTORY_MODE = S_IFDIR,
};

typedef unsigned int uint;

//////////////// MK COPING WITH THE LINTER, NEVERMIND THIS BLOCK ///////////////

#ifndef __unix__
#define F_WRLCK 1
#define F_SETLKW 1
#define F_UNLCK 1

char* strtok_r(char*, char*, char**);
struct flock {
    int l_type;
    int l_whence;
    int l_start;
    int l_len;
    int l_pid;
};

int fcntl(int, int, struct flock*);

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
char* strndup(const char*, size_t);
#endif
////////////////////////////// COPING BLOCK ENDS ///////////////////////////////


////////////////////////////////// ERROR CODES /////////////////////////////////

#define ITOPSC  0 // I-Table operation succeeded        | Success Code
#define ITOPFL -1 // I-Table operation failed (generic) | Failure Code
#define EITWNB -2 // I-Table Was Not Built              |  Error  Code
#define EITWR  -3 // I-Table Was Reset                  |  Error  Code
#define EITWNR -4 // I-Table Was Not Reset              |  Error  Code


#define FSOPSC  0  // File System operation succeeded   | Success Code
#define FSOPFL -1  // File System operation failed      | Failure Code
////////////////////////////////// ERROR CODES /////////////////////////////////


/////////////////////////// FUNCTION PROTOTYPES START //////////////////////////

void wfs_inode_init(struct wfs_inode* restrict inode, enum InodeModes mode);
int _check_dir_inode(struct wfs_inode* inode);
int _check_reg_inode(struct wfs_inode* inode);

void _check();
static inline off_t lookup_itable(uint);
static inline void fill_itable(uint, long);
static inline void invalidate_itable();
int set_itable_capacity(uint);
int build_itable();

static inline void invalidate_path_history();
int set_path_history_capacity(uint);

int find_file_in_dir(struct wfs_log_entry*, char*, uint*);
char* simplify_path(const char* restrict);
int parse_path(const char* restrict, uint* restrict);
struct wfs_log_entry* get_log_entry(uint);
int add_dentry(struct wfs_log_entry** entry, struct wfs_dentry* dentry);
int remove_dentry(struct wfs_log_entry** entry, struct wfs_dentry* dentry);

int read_from_disk(off_t, struct wfs_log_entry**);
int write_to_disk(off_t, struct wfs_log_entry*);
int append_log_entry(struct wfs_log_entry*);
int read_sb_from_disk();
int write_sb_to_disk();
void validate_disk_file();

void setup_flocks();
static inline void begin_op();
static inline void end_op();

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
    unsigned char is_valid: 2;
    unsigned char fsck: 2;
    unsigned char rebuilding: 4;
    uint n_inodes;
    uint n_log_entries;
    char* restrict disk_filename;
    FILE* restrict disk_file;
    struct flock wfs_lock;
    struct {
        long* restrict table; // This struct is like a cache for
        uint  capacity;       // faster lookups from the disk image.
    } itable;                 // This is set in build_itable().
    struct {
        uint* history;  // This records the path taken, to traverse
        uint capacity;  // backwards for relative paths.
    } path_history;     // Used only by parse_path
    struct wfs_sb sb; // This is set in validate_disk_file().
    uint cached_head;
} ps_sb = {
    .is_valid = 0,
    .fsck = 0,
    .rebuilding = 0,
    .n_inodes = WFS_INITIAL_INODE_NUMBER,
    .n_log_entries = 0,
    .disk_filename = NULL,
    .disk_file = NULL,
    .wfs_lock = {
        .l_type   = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start  =  0,
        .l_len    =  0,
        .l_pid    = -1,
    },
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
    .cached_head = 0,
};

////////////////////////// BOOKKEEPING VARIABLES END ///////////////////////////


/////////////////////// INODE MANAGEMENT FUNCTIONS START ///////////////////////

/**
 * Initialize an Inode
 *
 * @param inode The inode buffer to fill up
 * @param mode  The mode of the new inode
 */
void wfs_inode_init(struct wfs_inode* restrict inode, enum InodeModes mode) {
    *inode = (struct wfs_inode) {
        .inode_number = ps_sb.n_inodes++,
        .deleted      = WFS_INODE_INTACT,
        .mode         = mode,
        .uid          = WFS_USER_ID,
        .gid          = WFS_GROUP_ID,
        .flags        = WFS_IGNORED_FIELD,
        .size         = WFS_IGNORED_FIELD,
        .atime        = WFS_CURR_TIME,
        .mtime        = WFS_CURR_TIME,
        .ctime        = WFS_CURR_TIME,
        .links        = WFS_N_HARD_LINKS,
    };
}

/**
 * Checks if the inode is for a directory
 *
 * @param  inode  Pointer to the inode to check
 *
 * @return  0 if inode represents a directory, 1 otherwise
 */
int _check_dir_inode(struct wfs_inode* inode) {
    if (S_ISDIR(inode->mode))
        return 0;

    WFS_ERROR(
        "Given inode does not represent a directory\n"
        "{.inode_number = %d, .mode = %d, .ctime = %d}",
        inode->inode_number, inode->mode, inode->ctime
    );

    return 1;
}

/**
 * Checks if the inode is for a regular file
 *
 * @param  inode  Pointer to the inode to check
 *
 * @return  0 if inode represents a file, 1 otherwise
 */
int _check_reg_inode(struct wfs_inode* inode) {
    if (S_ISREG(inode->mode))
        return 0;

    WFS_ERROR(
        "Given inode does not represent a file\n"
        "{.inode_number = %d, .mode = %d, .ctime = %d}",
        inode->inode_number, inode->mode, inode->ctime
    );

    return 1;
}

//////////////////////// INODE MANAGEMENT FUNCTIONS END ////////////////////////


////////////////////// I-TABLE MANAGEMENT FUNCTIONS START //////////////////////

/**
 * Performs checks to verify in-memory data structures are intact
 */
void _check() {
    // FSCK performs it's own checks
    if (ps_sb.fsck || ps_sb.rebuilding)
        return;

    validate_disk_file();

    if (!ps_sb.is_valid) {
        WFS_ERROR("Cannot perform operation because given disk_file is not a valid wfs disk_file\n");
        exit(FSOPFL);
    }

    if (!ps_sb.disk_filename) {
        WFS_ERROR("No disk file is specified\n");
        exit(FSOPFL);
    }

    if (!ps_sb.disk_file) {
        WFS_ERROR("No FILE handle for the %s was found\n", ps_sb.disk_filename);
        WFS_ERROR("Retrying once to re-build.\n");

        ps_sb.disk_file = fopen(ps_sb.disk_filename, "r+");

        if (!ps_sb.disk_file || (build_itable() != ITOPSC)) {
            WFS_ERROR("Retry failed! Exiting!\n");
            exit(FSOPFL);
        }
    }
}

/**
 * Lookup the I-Table entries for the given inode
 * Rebuilds the I-Table if the given inode number is not found
 *
 * @param inode_number The inode number to lookup
 *
 * @return  The offset of the most recent log entry corresponding to this inode
 *          number
 */
static inline off_t lookup_itable(uint inode_number) {
    _check();

    if (inode_number > ps_sb.n_inodes || inode_number >= ps_sb.itable.capacity) {
        WFS_INFO("Inode %d was not found in the I-Table. Rebuilding...\n", inode_number);
        invalidate_itable();
        if (build_itable() != ITOPSC) {
            WFS_ERROR("Failed to re-build I-Table.\n");
            exit(ITOPFL);
        }
        WFS_INFO("I-Table was re-built successfully\n");
    }

    if (inode_number > ps_sb.n_inodes) {
        WFS_ERROR("Inode number exceeds total number of inodes %u > %u\n",
                  inode_number, ps_sb.n_inodes);
        return ITOPFL;
    }

    return ps_sb.itable.table[inode_number];
}

/**
 * Adds or updates itable entries for the given inode
 *
 * @param inode_number Inode this entry is for
 * @param offset       The offset of the most recent entry for the given inode
 */
static inline void fill_itable(uint inode_number, long offset) {
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
int set_itable_capacity(uint capacity) {
    off_t* temp;
    switch (ps_sb.itable.capacity) {
    case 0:
        if (ps_sb.itable.table)
            free(ps_sb.itable.table);

        ps_sb.itable.table = NULL;

        temp = calloc(capacity, sizeof(off_t));

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

        // Invalidate newly allocated memory
        int excess = capacity - ps_sb.itable.capacity;
        if (excess > 0)
            memset(temp + ps_sb.itable.capacity, 0, sizeof(off_t) * (excess));
    }

    ps_sb.itable.table = temp;
    ps_sb.itable.capacity = capacity;

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
int build_itable() {
    ps_sb.rebuilding = 1;
    _check();
    ps_sb.n_inodes = 0;
    ps_sb.n_log_entries = 0;

    begin_op();

    if(fseek(ps_sb.disk_file, sizeof(struct wfs_sb), SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        end_op();
        exit(ITOPFL);
    }

    long seek;

    while ((seek = ftell(ps_sb.disk_file)) < ps_sb.sb.head) {
        struct wfs_log_entry* entry = malloc(sizeof(struct wfs_log_entry));
        if (!entry) {
            WFS_ERROR("MALLOC FAILED!\n");
            end_op();
            ps_sb.rebuilding = 0;
            return ITOPFL;
        }

        if (fread(entry, sizeof(struct wfs_inode), 1, ps_sb.disk_file) != 1) {
            WFS_ERROR("fread Failed!\n");
            free(entry);
            end_op();
            ps_sb.rebuilding = 0;
            return ITOPFL;
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
                end_op();
                ps_sb.rebuilding = 0;
                return EITWNB;
            case EITWR:
                WFS_ERROR("Orignal data should be intact.\n");
                end_op();
                ps_sb.rebuilding = 0;
                return EITWR;
            case EITWNR:
                WFS_ERROR("Failed to restore old data.\n");
                invalidate_itable();
                end_op();
                ps_sb.rebuilding = 0;
                return EITWNR;
            }
        }

        fill_itable(inode_number, entry->inode.deleted ? 0l : seek);

        if (fseek(ps_sb.disk_file, data_size, SEEK_CUR)) {
            WFS_ERROR("fseek failed!\n");
            end_op();
            exit(ITOPFL);
        }

        free(entry);
    }

    end_op();

    if (*ps_sb.itable.table < WFS_INIT_ROOT_OFFSET) {
        WFS_ERROR("Didn't find root inode. Build Failed!\n");
        ps_sb.rebuilding = 0;
        return ITOPFL;
    }

    ps_sb.rebuilding = 0;
    return ITOPSC;
}

/////////////////////// I-TABLE MANAGEMENT FUNCTIONS END ///////////////////////


//////////////////// PATH HISTORY MANAGEMENT FUNCTIONS START ///////////////////

/**
 * If path history is NULL, does nothing.
 * If path history is an array of uints, sets all except index 0 to (uint) -1
 * (uint) -1 is the code that specified invalid entry
 */
static inline void invalidate_path_history() {
    if (!ps_sb.path_history.history || ps_sb.path_history.capacity == 0) {
        ps_sb.path_history.capacity = 0;
        set_path_history_capacity(PATH_HISTORY_CAPACITY_INCREMENT);
        return;
    }

    uint capacity = ps_sb.path_history.capacity;

    memset(ps_sb.path_history.history, (uint) -1, sizeof(uint) * capacity);
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
int set_path_history_capacity(uint capacity) {
    _check();

    uint* temp;
    switch (ps_sb.path_history.capacity) {
    case 0:
        if (ps_sb.path_history.history)
            free(ps_sb.path_history.history);

        temp = malloc(sizeof(uint) * capacity);
        if (!temp) {
            WFS_ERROR("Malloc Failed!\n");
            return FSOPFL;
        }
        memset(temp, (uint) -1, sizeof(uint) * capacity);
        break;
    default:
        temp = realloc(ps_sb.path_history.history, sizeof(uint) * capacity);
        if (!temp) {
            WFS_ERROR("Realloc failed!\n");
            return FSOPFL;
        }

        // Invalidate newly allocated memory
        int excess = capacity - ps_sb.itable.capacity;
        if (excess > 0)
            memset(temp + ps_sb.path_history.capacity, (uint) -1,
                   sizeof(off_t) * (excess));
    }

    ps_sb.path_history.history = temp;
    ps_sb.path_history.capacity = capacity;

    return FSOPSC;
}

///////////////////// PATH HISTORY MANAGEMENT FUNCTIONS END ////////////////////


//////////////////// FILE SYSTEM MANAGEMENT FUNCTIONS START ////////////////////

/**
 * Finds the inode number of the given filename in the wfs_log_entry
 * and stores it in the out param
 * @param  entry    A log entry corresponding to a directory
 *                  This means we should interpret entry.data as wfs_dentry[]
 * @param  filename The file to look for in the data section
 * @param  out      Where to store the inode number if the file is found.
 *                  Set to NULL if file not found
 *
 * @return  FSOPSC on success, FSOPFL on failure
 */
int find_file_in_dir(struct wfs_log_entry* entry, char* filename,
                            uint* out) {
    if(_check_dir_inode(&entry->inode))
        return FSOPFL;

    *out = (uint) -1;

    int n_entries = entry->inode.size / sizeof(struct wfs_dentry);

    struct wfs_dentry* dentry = (struct wfs_dentry*) entry->data;

    for (int i = 0; i < n_entries; i++, dentry++) {
        if (strcmp(filename, dentry->name) != 0)
            continue;

        dentry->name[MAX_FILE_NAME_LEN - 1] = 0;

        *out = dentry->inode_number;
        break;
    }

    if (*out == ((uint)-1))
        return FSOPFL;

    if (*out >= ps_sb.n_inodes) {
        WFS_ERROR(
            "Corrupted Data in WFS! Inode number %d "
            "exceeds total number of inodes %d\n"
            "Looking for file %s in Inode %d",
            *out, ps_sb.n_inodes, filename, entry->inode.inode_number
        );
        return FSOPFL;
    }

    return FSOPSC;
}

char* simplify_path(const char* restrict path) {
    ssize_t len = strlen(path);
    const char* s = path;
    const char* e = &path[len - 1];

    while (s < e && *s == '/') s++;
    while (e > s && *e == '/') e--;

    return strndup(s, e - s + 1);
}

/**
 * Given a path, this function will parse it to find the inode number
 * corresponding to the file or directory point to by this
 * @param  path The path to the file (shouldn't be > 128 characters)
 * @param  out  The address of the out variable
 *
 * @return FSOPSC on success, FSOPFL on failure
 */
int parse_path(const char* path, uint* out) {
    _check();
    invalidate_path_history();
    char* orig = NULL;
    uint inode_number;
    struct wfs_log_entry* entry = NULL;
    int ph_idx = 1;

    // All paths should start with a "/"
    if (*path != '/')
        goto fail;

    // If the filepath is literally the root directory, return inode 0
    if (strcmp("/", path) == 0)
        goto success;

    char* _path = simplify_path(path);
    orig = _path;

    // If after all stripping we're left with nothing (i.e. "///" -> "")
    if (*_path == '\0')
        goto success;

    char* base_filename = strrchr(_path, '/');

    // Special Case, File is in the root directory.
    if (!base_filename) {
        entry = get_log_entry(0);

        // Handle Dentry
        if (find_file_in_dir(entry, _path, &inode_number) != FSOPSC)
            goto fail;

        ps_sb.path_history.history[ph_idx++] = inode_number;
        goto success;
    }

    char* token;
    char* context;

    entry = get_log_entry(0); // Root log_entry
    if (!entry) {
        WFS_INFO("Didn't find root log_entry. Rebuilding I-Table\n");
        exit(ITOPFL);
    }

    while ((token = strtok_r(_path, "/", &context)) != NULL) {
        if (strcmp(".", token) == 0)
            goto next_token;
        else if (strcmp("..", token) == 0) {
            ph_idx -= ph_idx != 0;
            goto next_token;
        }

        if (find_file_in_dir(entry, token, &inode_number) != FSOPSC)
            goto fail;

        if (ps_sb.path_history.capacity <= ph_idx)
            set_path_history_capacity(ph_idx + PATH_HISTORY_CAPACITY_INCREMENT);

        ps_sb.path_history.history[ph_idx++] = inode_number;

        next_token:
        // Free allocated memory for previous log_entry
        free(entry);
        // Get log_entry corresponding to the inode_number
        if (!(entry = get_log_entry(ps_sb.path_history.history[ph_idx - 1]))) {
            WFS_ERROR("get_log_entry failed! (arg = %u)", ps_sb.path_history.history[ph_idx - 1]);
            goto fail;
        }

        _path = context;
    }

    success:
    *out = ps_sb.path_history.history[--ph_idx];
    if (orig)
        free(orig);
    if (entry)
        free(entry);
    return FSOPSC;

    fail:
    *out = (uint) -1;
    if (orig)
        free(orig);
    if (entry)
        free(entry);

    WFS_ERROR("Path %s is not a valid path!\n", path);
    return FSOPFL;
}

/**
 * Returns the most recent log_entry corresponding to the given inode_number
 *
 * @param  inode_number The inode number to look for
 *
 * @return Pointer to an in-memory representation of the log_entry (heap allocated)
 */
struct wfs_log_entry* get_log_entry(uint inode_number) {
    off_t offset = lookup_itable(inode_number);

    if (offset < WFS_INIT_ROOT_OFFSET) {
        WFS_ERROR("Inode Number %u has been deleted\n", inode_number);
        return NULL;
    }

    struct wfs_log_entry* entry = NULL;

    begin_op();
    read_from_disk(offset, &entry);
    end_op();

    if (!entry)
        WFS_ERROR("Read from disk at offset %ld failed!\n", offset);

    return entry;
}

/**
 * Add a new WFS Dentry to the given log entry
 * This can fail if a dentry with the same name already exists
 *
 * @param  entry  The log entry to extend with the given dentry
 * @param  dentry The dentry to add
 *
 * @return  FSOPSC on success, FSOPFL on failure
 */
int add_dentry(struct wfs_log_entry** entry, struct wfs_dentry* dentry) {
    // First we check if dentry already exists
    int n_entries = (*entry)->inode.size / sizeof(struct wfs_dentry);

    struct wfs_dentry* dentries = (struct wfs_dentry*) (*entry)->data;

    for (int i = 0; i < n_entries; i++)
        if (0 == strcmp(dentry->name, dentries[i].name))
            return FSOPFL;
    
    size_t new_size = WFS_LOG_ENTRY_SIZE(*entry) + sizeof(struct wfs_dentry);
    struct wfs_log_entry* temp = realloc(*entry, new_size);

    if (!temp) {
        WFS_ERROR("realloc failed!");
        return FSOPFL;
    }

    *entry = temp;

    dentries = (struct wfs_dentry*) (*entry)->data;
    dentries[n_entries] = *dentry;

    (*entry)->inode.size += sizeof(struct wfs_dentry);

    return FSOPSC;
}

/**
 * Remove a WFS Dentry from the given log entry
 *
 * @param  entry  Log Entry that contains the given dentry
 * @param  dentry The Dentry to remove
 *
 * @return  FSOPSC on success, FSOPFL on failure
 */
int remove_dentry(struct wfs_log_entry** entry, struct wfs_dentry* dentry) {
    int n_entries = (*entry)->inode.size / sizeof(struct wfs_dentry);

    struct wfs_dentry* dentries = (struct wfs_dentry*) (*entry)->data;

    int i;

    for (i = 0; i < n_entries; i++)
        if (0 == strcmp(dentry->name, dentries[i].name))
            break;

    if (i >= n_entries)
        return FSOPFL;

    for (int j = i; j < n_entries - 1; j++)
        dentries[j] = dentries[j + 1];

    size_t new_size = WFS_LOG_ENTRY_SIZE(*entry) - sizeof(struct wfs_dentry);

    struct wfs_log_entry* temp = realloc(*entry, new_size);

    if (!temp) {
        WFS_ERROR("realloc failed!");
        return FSOPFL;
    }

    *entry = temp;
    (*entry)->inode.size -= sizeof(struct wfs_dentry);

    return FSOPSC;
}

//////////////////// FILE SYSTEM MANAGEMENT FUNCTIONS START ////////////////////


///////////////////// DISK FILE MANAGEMENT FUNCTIONS START /////////////////////

/**
 * Reads the log_entry at the given offset from the disk image
 *
 * Assumes locks are held by callee
 *
 * @param offset    The offset of the start of the log_entry
 * @param entry_buf The log_entry buffer to fill. Will be heap allocated.
 *
 * returns FSOPSC on success, FSOPFL on failure
 */
int read_from_disk(off_t offset, struct wfs_log_entry** entry_buf) {
    _check();
    *entry_buf = malloc(sizeof(struct wfs_log_entry));

    long pos = ftell(ps_sb.disk_file);

    if (fseek(ps_sb.disk_file, offset, SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        return FSOPFL;
    }

    if (fread(*entry_buf, sizeof(struct wfs_inode), 1, ps_sb.disk_file) != 1)
        goto fail;

    int size = (*entry_buf)->inode.size;
    int new_size = sizeof(struct wfs_log_entry) + (sizeof(char) * size);

    struct wfs_log_entry* temp = realloc(*entry_buf, new_size);

    if (!temp) {
        WFS_ERROR("Realloc failed!\n");
        goto fail;
    }

    *entry_buf = temp;

    if (fread(&(*entry_buf)->data, sizeof(char), size, ps_sb.disk_file) != size)
        goto fail;

    (*entry_buf)->inode.atime = WFS_CURR_TIME;
    (*entry_buf)->inode.ctime = WFS_CURR_TIME;

    if (fseek(ps_sb.disk_file, offset, SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        return FSOPFL;
    }

    if(fwrite(*entry_buf, sizeof(struct wfs_inode), 1, ps_sb.disk_file) < 1) {
        WFS_ERROR("fwrite failed!\n");
        return FSOPFL;
    }

    if (fseek(ps_sb.disk_file, pos, SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        return FSOPFL;
    }

    return FSOPSC;

    fail:
    if (*entry_buf)
        free(*entry_buf);

    *entry_buf = NULL;
    return FSOPFL;
}

/**
 * Writes the log_entry at the given offset to the disk image
 *
 * Assumes locks are held by callee
 *
 * @param offset    The offset of file to write to
 * @param entry_buf The log_entry to write to the file
 *
 * @return  FSOPSC on success,
 *          -ENOSPC if disk_file doesn't have enough space
 *          FSOPFL on failure of any other type
 */
int write_to_disk(off_t offset, struct wfs_log_entry* entry) {
	_check();

    size_t size = WFS_LOG_ENTRY_SIZE(entry);

    if ((offset + size) > MAX_DISK_FILE_SIZE)
        return -ENOSPC;

    long pos = ftell(ps_sb.disk_file);

    if (fseek(ps_sb.disk_file, offset, SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        return FSOPFL;
    }

    entry->inode.mtime = WFS_CURR_TIME;
    entry->inode.ctime = WFS_CURR_TIME;

    if(fwrite(entry, size, 1, ps_sb.disk_file) < 1) {
        WFS_ERROR("fwrite failed!\n");
        return FSOPFL;
    }

    if (fflush(ps_sb.disk_file) != 0) {
        WFS_ERROR("fflush failed!\n");
        return FSOPFL;
    }

    if (fdatasync(fileno(ps_sb.disk_file)) != 0) {
        WFS_ERROR("fdatasync failed!\n");
        return FSOPFL;
    }

    if (fseek(ps_sb.disk_file, pos, SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        return FSOPFL;
    }

    if ((offset + size) >= ps_sb.sb.head)
        ps_sb.sb.head = offset + size;

    write_sb_to_disk();

    fill_itable(entry->inode.inode_number, offset);

    return FSOPSC;
}

/**
 * Append a log entry to the disk image
 *
 * This is just a convenient wrapper function over write_to_disk
 * to append a new log entry at the end.
 *
 * Assumes locks are held by callee
 *
 * @param  entry The log entry to append
 *
 * @return  0 on success, -ENOSPC if disk_file doesn't have enough space
 */
int append_log_entry(struct wfs_log_entry* entry) {
    return write_to_disk(ps_sb.sb.head, entry);
}

/**
 * Reads the superblock from the disk image
 */
int read_sb_from_disk() {
    // Store initial offset
    long pos = ftell(ps_sb.disk_file);

    if (fseek(ps_sb.disk_file, 0, SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        return FSOPFL;
    }

    if(fread(&ps_sb.sb, sizeof(struct wfs_sb), 1, ps_sb.disk_file) != 1) {
        WFS_ERROR("fread failed!\n");
        exit(ITOPFL);
    }

    // Restore initial offset
    if (fseek(ps_sb.disk_file, pos, SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        return FSOPFL;
    }

    return FSOPSC;
}

/**
 * Writes a superblock to the disk image
 */
int write_sb_to_disk() {
    if (ps_sb.sb.magic != WFS_MAGIC) {
        WFS_ERROR("In memory superblock is invalid. "
                  "Expected Magic = %x, Actual = %x\n"
                  "WFS will abort after checking disk file status\n",
                  WFS_MAGIC, ps_sb.sb.magic);

        WFS_INFO("Verifying disk file...\n");

        validate_disk_file();

        WFS_INFO("Determined disk image to be valid. "
                 "However this is an unrecoverable error. ABORTING!\n");

        return FSOPFL;
    }

    if (ps_sb.sb.head < WFS_INIT_ROOT_OFFSET || ps_sb.sb.head > MAX_DISK_FILE_SIZE) {
        WFS_ERROR("In memory superblock is invalid. "
                  "Expected Head to be greater than or equal to %x, Actual = %x\n"
                  "This may be a recoverable error.\n"
                  "Verifying disk file...\n",
                  (uint) WFS_INIT_ROOT_OFFSET, ps_sb.sb.magic);

        validate_disk_file();

        WFS_INFO("Determined disk image to be valid. (head = %x)", ps_sb.sb.head);

        if (ps_sb.itable.table != NULL
                && ps_sb.itable.capacity > 0
                && ps_sb.n_inodes < ps_sb.itable.capacity) {
            WFS_INFO("Cache is available. Recomputing head using cache.\n");

            off_t max = WFS_INIT_ROOT_OFFSET;
            uint max_idx = -1;

            for (uint i = 0; i < ps_sb.n_inodes; i++) {
                off_t off = lookup_itable(i);
                if (off > max) {
                    max = off;
                    max_idx = i;
                }
            }

            if (max_idx == -1) {
                WFS_ERROR("Cache is invalid. Aborting\n");
                return FSOPFL;
            }

            struct wfs_log_entry* entry = get_log_entry(max_idx);

            if (!entry) {
                WFS_ERROR("Cache is invalid. Aborting\n");
                return FSOPFL;
            }

            max = entry->inode.size;
            free(entry);

            if (max != ps_sb.sb.head) {
                WFS_INFO("Head on disk is not equal to the one computed from"
                         " cache (%x != %x). "
                         "Setting the head to the **larger** value!",
                         ps_sb.sb.head, (unsigned int) max);

                max = max > ps_sb.sb.head ? max : ps_sb.sb.head;
            }

            ps_sb.sb.head = max;
        } else {
            WFS_INFO("Cache is unavailable. Aborting!");
            return FSOPFL;
        }
    }

    long pos = ftell(ps_sb.disk_file);

    if (fseek(ps_sb.disk_file, 0, SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        return FSOPFL;
    }

    if (fwrite(&ps_sb.sb, sizeof(struct wfs_sb), 1, ps_sb.disk_file) != 1) {
        WFS_ERROR("fwrite failed!\n");
        return FSOPFL;
    }

    if (fflush(ps_sb.disk_file) != 0) {
        WFS_ERROR("fflush failed!\n");
        return FSOPFL;
    }

    if (fdatasync(fileno(ps_sb.disk_file)) != 0) {
        WFS_ERROR("fdatasync failed!\n");
        return FSOPFL;
    }

    if (fseek(ps_sb.disk_file, pos, SEEK_SET)) {
        WFS_ERROR("fseek failed!\n");
        return FSOPFL;
    }

    return FSOPSC;
}

/**
 * Parses the superblock of the given disk file to ensure the file is a valid
 * WFS disk image
 *
 * Exits with a failure code if the given file is invalid
 */
void validate_disk_file() {
    ps_sb.is_valid = 0;

    read_sb_from_disk();

    if (ps_sb.sb.magic != WFS_MAGIC) {
        WFS_ERROR("File \"%s\" is not a valid WFS FileSystem disk image. "
                  "(magic = %x)\n",
                  ps_sb.disk_filename, ps_sb.sb.magic);
        return;
    }

    if (ps_sb.sb.head < WFS_INIT_ROOT_OFFSET) {
        WFS_ERROR("File \"%s\" is not a valid WFS FileSystem disk image. "
                  "(head = %x)\n",
                  ps_sb.disk_filename, ps_sb.sb.head);
        return;
    }

    if (ps_sb.cached_head != ps_sb.sb.head && !ps_sb.rebuilding) {
        WFS_INFO("Current head doesn't match cached head. Rebuilding I-Table...\n");
        if(build_itable() != ITOPSC) {
            WFS_ERROR("Failed to re-build I-Table\n");
            return;
        }
        WFS_INFO("Re-built I-Table successfully!\n");
        ps_sb.cached_head = ps_sb.sb.head;
    }

    ps_sb.is_valid = 1;
}

////////////////////// DISK FILE MANAGEMENT FUNCTIONS END //////////////////////


///////////////////////// FILE CONTROL FUNCTIONS START /////////////////////////

/**
 * Acquire file locks
 */
static inline void begin_op() {
    ps_sb.wfs_lock.l_type = F_WRLCK;
    int ret;
    do {
        ret = fcntl(fileno(ps_sb.disk_file), F_SETLKW, &ps_sb.wfs_lock);
    } while (ret == -1  && errno == EINTR);

    if (ret == -1) {
        WFS_ERROR("FLock Lock failed! (err: %s)", strerror(errno));
        exit(FSOPFL);
    }
}

/**
 * Release file locks
 */
static inline void end_op() {
    int ret;
    do {
        ps_sb.wfs_lock.l_type = F_UNLCK;
        ret = fcntl(fileno(ps_sb.disk_file), F_SETLKW, &ps_sb.wfs_lock);
    } while (ret == -1 && errno == EINTR);

    if (ret == -1) {
        WFS_ERROR("FLock Unlock failed! (err: %s)", strerror(errno));
        exit(FSOPFL);
    }
}

////////////////////////// FILE CONTROL FUNCTIONS END //////////////////////////


////////////////////// WFS INITIALIZATION FUNCTIONS START //////////////////////

/**
 * Initialize a Superblock
 *
 * @param sb The superblock buffer to fill up
 */
void wfs_sb_init(struct wfs_sb* restrict sb) {
    sb->magic = WFS_MAGIC;
    sb->head = (uint32_t) WFS_BASE_ENTRY_OFFSET;
}

/**
 * Sets up file locks to work with multiple processes
 */
void setup_flocks() {
    _check();
    ps_sb.wfs_lock.l_pid = getpid();
}

/**
 * Initializes WFS for a `program` with the given disk `filename`
 *
 * @param program  The program to initialize WFS for
 * @param filename The path to the disk file
 */
void wfs_init(const char* program, const char* filename) {
    ps_sb.fsck = strstr(program, "fsck.wfs") != NULL;
    ps_sb.rebuilding = 1;

    ps_sb.disk_filename = strdup(filename);
    if(!ps_sb.disk_filename){
        WFS_ERROR("strdup failed\n");
        exit(ITOPFL);
    }
    WFS_INFO("disk_path = %s\n", ps_sb.disk_filename);
    ps_sb.disk_file = fopen(ps_sb.disk_filename, "r+");

    if (!ps_sb.disk_file) {
        WFS_ERROR("Couldn't open file \"%s\"\n", ps_sb.disk_filename);
        exit(FSOPFL);
    }

    invalidate_itable();
    invalidate_path_history();

    validate_disk_file();

    if (!ps_sb.is_valid) {
        WFS_ERROR("Disk File validation failed! (disk_file: \"%s\")\n",
                  ps_sb.disk_filename);
        exit(FSOPFL);
    }

    ps_sb.cached_head = ps_sb.sb.head;

    setup_flocks();

    WFS_INFO("Building I-Table...\n");

    int err;
    if((err = build_itable(ps_sb.disk_file)) != ITOPSC) {
        WFS_ERROR("Failed to build I-Table | Error Code: %d\n", err);
        exit(FSOPFL);
    }

    WFS_INFO("Built I-Table successfully!\n");
    WFS_INFO("Parsed %d log entries, with %d inodes\n", ps_sb.n_log_entries, ps_sb.n_inodes);

    WFS_INFO("WFS Initialized successfully!\n");
    ps_sb.rebuilding = 0;
}

/////////////////////// WFS INITIALIZATION FUNCTIONS END ///////////////////////

#endif
