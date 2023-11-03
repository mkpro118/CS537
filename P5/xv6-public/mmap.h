#ifndef _XV6_MMAP_
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

/* NUMBER OF MMAPS*/
#define N_MMAPS 0x20

/* Macros to check flags */
#define IS_MMAP_PRIVATE(flags) (((flags) & MAP_PRIVATE) > 0)
#define IS_MMAP_SHARED(flags) (((flags) & MAP_SHARED) > 0)
#define IS_MMAP_ANON(flags) (((flags) & MAP_ANON) > 0)
#define IS_MMAP_FIXED(flags) (((flags) & MAP_FIXED) > 0)
#define IS_MMAP_GROWSUP(flags) (((flags) & MAP_GROWSUP) > 0)

struct mmap {
  unsigned int is_valid : 1;
  unsigned int prot : 2;
  unsigned int flags : 5;

  unsigned int length;

  void* start_addr;
  void* end_addr;

  int fd;
//  struct file* mfile;

  unsigned int refcount;
};

/* MMAP INIT MACRO */
#define MMAP_INIT(mp, prot, flags, length, start, end, fd, refcount) \
  (mp)->is_valid = 1;\
  (mp)->prot = (prot);\
  (mp)->flags = (flags);\
  (mp)->length = (length);\
  (mp)->start_addr = (start);\
  (mp)->end_addr = (end);\
  (mp)->fd = (fd);\
  (mp)->refcount = (refcount);


#endif
