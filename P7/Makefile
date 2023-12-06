NAME = mount.wfs mkfs.wfs fsck.wfs
DEBUG_NAME = debug

CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18 -g -fsanitize=address
FUSE_CFLAGS = `pkg-config fuse --cflags --libs`
DEBUG_CFLAGS = -DWFS_DBUG=1
MMAP_CFLAGS = -DWFS_MMAP

.PHONY: all
all: $(NAME)
	./create_disk.sh
	./mkfs.wfs disk
	-./umount.wfs mnt

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
	rm -fr *.wfs.debug
	rm -fr *.wfs.mmap

.PHONY: debug
debug:
	#./create_disk.sh
	#rm -fr compact_disk outfile
	#yes | cp prebuilt_disk compact_disk
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) mount.wfs.c $(FUSE_CFLAGS) -o mount.wfs.debug
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) -o mkfs.wfs.debug mkfs.wfs.c
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) -o fsck.wfs.debug fsck.wfs.c

mmap:
	$(CC) $(CFLAGS) $(MMAP_CFLAGS) mount.wfs.c $(FUSE_CFLAGS) -o mount.wfs.mmap
	$(CC) $(CFLAGS) $(MMAP_CFLAGS) -o mkfs.wfs.mmap mkfs.wfs.c
	$(CC) $(CFLAGS) $(MMAP_CFLAGS) -o fsck.wfs.mmap fsck.wfs.c

.PHONY: test
test:
	~cs537-1/tests/P7/start.py

