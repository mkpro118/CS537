///////////////////////////////////////////////////////////////////////////////
///          CHANGES TO THIS FILE MUST BE DONE IN APPEND MODE ONLY          ///
///         DO NOT REMOVE ANY CONTENT FROM THIS FILE, ONLY ADD TO IT        ///
///             KEEP ALL TEXT AS COMMENTS, EXCEPT CODE SNIPPETS             ///
///////////////////////////////////////////////////////////////////////////////


// Data can be stored from log entries directly as bytes
// We can ead log_entries by first reading in inodes,
// then, figure out the size of the entire thing
// by reading inode.size
// Then we can read the entire struct

// fseek will come in handy

// All file operations are to be done in binary mode
// All writes are to be done in append mode

// Have to monitor file size
// say, using FILE* f
//
// ```
// fseek(f, 0, SEEK_END) -> Go to end of the file. Actually, maybe we should go to wfs_sb.head?
// pos = ftell(f)        -> Get the position of the end
//
// if sizeof(log_entry) + sizeof(buffer) + pos > 1MB, DO NOT write
// ```

// use fread, fwrite, fopen and fclose.
// DO NOT use syscall versions

// disk image is going to look like
//
// ---------- SUPERBLOCK ----------
// bytes 00-04: 0xdeadbeef             (is this like a FS identifier??)
// bytes 04-08: Next empty spot        (probably to be used with fseek)
// -------- END SUPERBLOCK --------
//
// --------- LOG ENTRY 0 ----------
// bytes 08-40: struct wfs_inode
// bytes 40-??: data
// ------- END LOG ENTRY 0 --------
//
// --------- LOG ENTRY 1 ----------
// bytes ??-(+40): struct wfs_inode
// bytes (+40)-(+40+??): data
// ------- END LOG ENTRY 1 --------
//                .
//                .
//                .
// --------- LOG ENTRY N ----------
// bytes ??-(+40): struct wfs_inode
// bytes (+40)-(+40+??): data
// ------- END LOG ENTRY N --------


// ABOUT INODES
// 1. wfs_inode.inode_number    | inode identifier
//      For every operation, we need to increment inode number
//      NEED a static counter, perhaps in a struct initializer function?
//      Or.. do we need to infer from the disk image, in order to persist state
//      between different runs of the FS?
//      Actually we may really need to read from the disk image to know. smh
//      It is actually super annoying that we can't modify structs this time
//
// 2. wfs_inode.deleted         | 1 if deleted, 0 otherwise
//      pretty straightforward, everytime we update, set the prev to 1
//
// 3. wfs_inode.mode            | type. S_IFDIR or S_IFREG
//      again, pretty straightforward, just indicates if
//      it refers to files or directories (#include <sys/stat.h>)
//
// 4. wfs_inode.uid             | user id
//      Use getuid() to get the user id, set the value here (#include <unistd.h>)
//
// 5. wfs_inode.gid             | group id
//      Use getgid() to get the group id, set the value here (#include <unistd.h>)
//
// 6. wfs_inode.size            | size in bytes
//      size here is actually size of the data entry for the log entry.
//      We have to use this to read bytes in the disk image file
//
// 7. wfs_inode.atime           | last access time
//      Use time() to set the atime every read op (#include <time.h>)
//
// 8. wfs_inode.mtime           | last modify time
//      Use time() to set the mtime every write op (#include <time.h>)


// MK's Proposed idea for working with the "disk image file"
// In this proposal, the "disk image file" will be referred to as FS (FileSystem, yeah shocker)
//
// This relies on viewing the disk image as shown above
//
// Before discussing operations, I'd like to propose 2 methods of FS traversal
// 1. Top to Bottom traversal.
//       This is the simplest way.
//       We start from the first log entry, which is found at SEEK_SET + sizeof(struct wfs_sb)
//       while (log_entry is not found)
//          read log entry inode, fread with size = sizeof(struct wfs_inode)
//          with inode information, we will know if the data field is a wsf_dentry
//          or the contents of a file
//          We will proceed with that info to do whatever
//
// 2. Bottom to Top traversal.
//      This is currently my preferred way, but it going to be
//      excrutiatingly difficult to figure out, considering we're not allowed to modify
//      the existsing structs. (otherwise it's ez)
//      basically we do the same thing as above, except we start reading from the end
//      and we offset from SEEK_END - sizeof(wfs_log_entry.data)
//      The only problem is, data is a field that has 0 size.
//      so we have to figure out another way to know the size of the data field.
//      How are we gonna do that? no idea.
//
//      oh if you're wondering why this is the preferred way, it's more efficient
//      since newer entries are appended to the end of the file,
//      it makes sense to start reading from there, we'll likely reach the log_entry
//      faster. but oh my gosh I cannot figure out a way to store this info about log_entries
//      without modifying the structs, or embedding this info somewhere in the file.
//      the problem with embedding it in the file is the storage takes up space which could
//      have been used for other log entries [O(n) auxillary storage.
//      Classic time vs storage tradeoff!

// Function to read data from a file,
// THIS IS ONLY A STARTER VERSION
// Doesn't really work, we have to figure out a way
// to know what the size of the data is for the last entry
// in order to support reverse reads.
// ofc we don't have to do that, it's for efficiency :)
// Actually, every log entry needs to have some sort of record
// for how long the previous entry is (except the first one obvi, but that can be 0)
ssize_t wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Open the disk image file
    FILE *file = fopen(disk_path, "rb");
    if (file == NULL) {
        return -errno;
    }

    // Seek to the end of the file
    fseek(file, 0, SEEK_END);
    off_t end = ftell(file);

    // Scan backwards through the log
    for (off_t i = end - sizeof(struct wfs_inode); i >= 0; i -= sizeof(struct wfs_inode)) {
        // Seek to the current log entry
        fseek(file, i, SEEK_SET);

        // Read the inode
        struct wfs_inode inode;
        fread(&inode, sizeof(inode), 1, file);

        // Check if the inode corresponds to the file
        if (strcmp(inode.name, path) == 0) {
            // We found the file, allocate memory for the log entry
            struct wfs_log_entry *entry = malloc(sizeof(struct wfs_log_entry) + inode.size);

            // Seek back to the start of the log entry
            fseek(file, i, SEEK_SET);

            // Read the log entry
            fread(entry, sizeof(struct wfs_log_entry) + inode.size, 1, file);

            // Copy the data into the buffer
            memcpy(buf, entry->data + offset, size);

            // Clean up and return the number of bytes read
            free(entry);
            fclose(file);
            return size;
        }
    }

    // If we get here, we didn't find the file
    fclose(file);
    return -ENOENT;
}
