#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#ifndef MOUNT_WFS_H_
#define MOUNT_WFS_H_

#define MAX_FILE_NAME_LEN 32
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
    fprintf(stdout, ":DEBUG (%d):  ", __LINE__);\
    fprintf(stdout, __VA_ARGS__);\
} while(0)

#define PRINT_INODE(x) do {\
    printf("\n{\n");\
    printf("  inode_number = %u\n", x->inode_number);\
    printf("  deleted      = %u\n", x->deleted);\
    printf("  mode         = %u\n", x->mode);\
    printf("  uid          = %u\n", x->uid);\
    printf("  gid          = %u\n", x->gid);\
    printf("  flags        = %u\n", x->flags);\
    printf("  size         = %u\n", x->size);\
    printf("  atime        = %u\n", x->atime);\
    printf("  mtime        = %u\n", x->mtime);\
    printf("  ctime        = %u\n", x->ctime);\
    printf("  links        = %u\n", x->links);\
    printf("}\n");\
} while(0)

#define PRINT_LOG_ENTRY(x) do {\
    PRINT_INODE((&(x)->inode));\
\
    if (S_ISREG(((x)->inode.mode))) printf("Contents: %s\n", (x)->data);\
    else if (S_ISDIR(((x)->inode.mode))) printf("=> Directory\n");\
    else printf("UNSUPPORTED TYPE\n");\
} while(0)

#else

#define WFS_DEBUG(...) (void)0

#endif

#define WFS_INFO(...) do {\
    fprintf(stdout, ":Info:  ");\
    fprintf(stdout, __VA_ARGS__);\
} while(0)

#define WFS_ERROR(...) do {\
    fprintf(stderr, ":ERROR:  ");\
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

enum InodeModes {
    FILE_MODE = S_IFREG,
    DIRECTORY_MODE = S_IFDIR,
};

void wfs_sb_init(struct wfs_sb* restrict sb) {
    sb->magic = WFS_MAGIC;
    sb->head = (uint32_t) WFS_BASE_ENTRY_OFFSET;
}

void wfs_inode_init(struct wfs_inode* restrict inode, enum InodeModes mode) {
    static unsigned int inode_number = WFS_INITIAL_INODE_NUMBER;
    *inode = (struct wfs_inode) {
        .inode_number = inode_number++,
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

int _check_dir_inode(struct wfs_inode* inode) {
    switch (inode->mode) {
    case DIRECTORY_MODE:
        return 0;
    default:
        WFS_ERROR(
            "Given inode does not represent a directory\n"
            "{.inode_number = %d, .mode = %d, .ctime = %d}",
            inode->inode_number, inode->mode, inode->ctime
        );
    }
    return 1;
}

int _check_reg_inode(struct wfs_inode* inode) {
    switch (inode->mode) {
    case FILE_MODE:
        return 0;
    default:
        WFS_ERROR(
            "Given inode does not represent a file\n"
            "{.inode_number = %d, .mode = %d, .ctime = %d}",
            inode->inode_number, inode->mode, inode->ctime
        );
    }
    return 1;
}

#endif
