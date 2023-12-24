NAME = mount.wfs mkfs.wfs fsck.wfs
DEBUG_NAME = debug

CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18 -g -fsanitize=address
FUSE_CFLAGS = `pkg-config fuse --cflags --libs`
DEBUG_CFLAG = -DWFS_DBUG=1
NOMMAP_CFLAG = -DWFS_NOMMAP
EXTEND_CFLAG = -DWFS_EXE=1

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
	rm -fr *.wfs.nommap
	rm -fr *.wfs.x

.PHONY: debug
debug:
	#./create_disk.sh
	#rm -fr compact_disk outfile
	#yes | cp prebuilt_disk compact_disk
	$(CC) $(CFLAGS) $(DEBUG_CFLAG) mount.wfs.c $(FUSE_CFLAGS) -o mount.wfs.debug
	$(CC) $(CFLAGS) $(DEBUG_CFLAG) -o mkfs.wfs.debug mkfs.wfs.c
	$(CC) $(CFLAGS) $(DEBUG_CFLAG) -o fsck.wfs.debug fsck.wfs.c

nommap:
	$(CC) $(CFLAGS) $(NOMMAP_CFLAG) mount.wfs.c $(FUSE_CFLAGS) -o mount.wfs.nommap
	$(CC) $(CFLAGS) $(NOMMAP_CFLAG) -o mkfs.wfs.nommap mkfs.wfs.c
	$(CC) $(CFLAGS) $(NOMMAP_CFLAG) -o fsck.wfs.nommap fsck.wfs.c

x:
	$(CC) $(CFLAGS) $(EXTEND_CFLAG) mount.wfs.c $(FUSE_CFLAGS) -o mount.wfs.x
	$(CC) $(CFLAGS) $(EXTEND_CFLAG) -o mkfs.wfs.x mkfs.wfs.c
	$(CC) $(CFLAGS) $(EXTEND_CFLAG) -o fsck.wfs.x fsck.wfs.c

.PHONY: test
test:
	~cs537-1/tests/P7/start.py

