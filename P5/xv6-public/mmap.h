#ifndef
#define _XV6_MMAP_

/* Define mmap flags */
#define MAP_PRIVATE 0x0001
#define MAP_SHARED 0x0002
#define MAP_ANONYMOUS 0x0004
#define MAP_ANON MAP_ANONYMOUS
#define MAP_FIXED 0x0008
#define MAP_GROWSUP 0x0010

/* Protections on memory mapping */
#define PROT_READ 0x1
#define PROT_WRITE 0x2

/* BASE LIMIT FOR MMAP */
#define MMAP_BASE 0x60000000

struct vir_mem {
  int is_valid : 1;
  int prot: 2;
  int flags: 5;

  int length;

  void* start_ad;
  void* end_ad;

  int fd;
  struct file* file;
};

#endif
