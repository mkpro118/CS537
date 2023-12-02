NAME = mount.wfs mkfs.wfs fsck.wfs
DEBUG_NAME = debug

CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18
FUSE_CFLAGS = `pkg-config fuse --cflags --libs`
DEBUG_CFLAGS = -DWFS_DBUG=1 -g -fsanitize=address

.PHONY: all
all: $(DEBUG_NAME)

.PHONY: mount.wfs
mount.wfs:
	$(CC) $(CFLAGS) mount.wfs.c $(FUSE_CFLAGS) -o mount.wfs

.PHONY: mkfs.wfs
mkfs.wfs:
	$(CC) $(CFLAGS) -o mkfs.wfs mkfs.wfs.c

.PHONY: fsck.wfs
fsck.wfs:
	$(CC) $(CFLAGS) -o fsck.wfs fsck.wfs.c

.PHONY: clean
clean:
	rm -rf $(NAME)

.PHONY: debug
debug:
	rm -fr compact_disk outfile
	yes | cp prebuilt_disk compact_disk
	truncate -s 1M outfile
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) mount.wfs.c $(FUSE_CFLAGS) -o mount.wfs
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) -o mkfs.wfs mkfs.wfs.c
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) -o fsck.wfs fsck.wfs.c
