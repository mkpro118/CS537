//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "memlayout.h"
#include "mmap.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// define global min var 
int freeadr = MMAP_BASE;
// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int sys_mmap(void) {
  uint addr, end;
  int length, prot, flags, fd;
  addr = MMAP_BASE;

  if (argint(1, &length) < 0
        || argint(2, &prot)  < 0
        || argint(3, &flags) < 0
        || argint(4, &fd) < 0) {
    goto mmap_failed;
  }

  // Check arguments
  if (length <= 0)
    goto mmap_failed;

  if (!(IS_MMAP_PRIVATE(flags) ^ IS_MMAP_SHARED(flags)))
    goto mmap_failed;

  if (!IS_MMAP_ANON(flags) && ((int) fd) < 0)
    goto mmap_failed;

  struct mmap* mp;
  struct proc* p = myproc();

  // Find a spot in the array to store this mmap call data
  for (mp = p->mmaps; mp < &p->mmaps[N_MMAPS]; mp++)
    if (!mp->is_valid)
      goto found_slot;

  // No more available mmaps
  goto mmap_failed;

  found_slot:
  // If MAP_FIXED is given
  if (IS_MMAP_FIXED(flags))
    goto mmap_fixed;

  // Otherwise, find a spot in memory.
  addr = MMAP_BASE;
  
  end = PGROUNDUP(addr + length);

  // If grows up, add another page for guard page
  if (IS_MMAP_GROWSUP(flags))
    end += PGSIZE;

  if (KERNBASE <= end)
    goto mmap_failed;

  struct mmap* mp2;

  // while we don't exceed KERNBASE
  while (end < KERNBASE) {
    // Go over the mmaps to see that if any of them lie in the range
    // [addr, end)
    for (mp2 = p->mmaps; mp2 < &p->mmaps[N_MMAPS]; mp2++)
      if (mp2->is_valid && mp2->start_addr >= addr && mp2->start_addr < end)
        goto retry;

    // Found an address, it is `addr`
    goto found_addr;

    // `addr` didn't work
    retry:
    addr = mp2->end_addr;
    end = PGROUNDUP(addr + length);

    // If grows up, add another page for guard page
    if (IS_MMAP_GROWSUP(flags))
     end += PGSIZE;
  }

  // Reached KERNBASE, no more memory
  goto mmap_failed;

  // addr was given, check if it's available
  mmap_fixed:
  if (argint(0, (int*) &addr) < 0)
    goto mmap_failed;

  // Bounds check, given address must lie in MMAP_BASE and KERNBASE
  if (MMAP_BASE > addr || KERNBASE <= addr)
    goto mmap_failed;

  end = PGROUNDUP(addr + length);

  // If grows up, add another page for guard page
  if (IS_MMAP_GROWSUP(flags))
   end += PGSIZE;

  if (KERNBASE <= end)
    goto mmap_failed;

  for (mp2 = p->mmaps; mp2 < &p->mmaps[N_MMAPS]; mp2++) {
    if (!mp2->is_valid)
      continue;

    // Three cases are possible
    // 1. A mmap region's start (but not end) lies in range [addr, end]
    // 2. A mmap region's end (but not start) lies in range [addr, end]
    // 3. A mmap region completely surrounds the requested region
    //    i.e, [addr, end] lies in the range [mp2->start_addr, mp2->end_addr]
    if ((mp2->start_addr >= addr && mp2->start_addr < end) /* Case 1*/
          || (mp2->end_addr > addr && mp2->end_addr <= end) /* Case 2*/
          || (mp2->start_addr <= addr && mp2->end_addr > end)) /* Case 3*/
      goto mmap_failed;
  }

  // addr exists
  found_addr:
  // First mmap, refcount is 1.
  int refcount = 1;

  // Get file if needed
  if (!(IS_MMAP_ANON(flags))) {
    if (argfd(4, &fd, &mp->f) < 0)
      goto mmap_failed;
 
    mp->f = filedup(mp->f);
  } else {
    mp->f = 0;
  }

  // Initialize bookeeper struct mmap
  MMAP_INIT(mp, prot, flags, length, addr, end, fd, refcount);

  // Success
  return mp->start_addr;

  mmap_failed:
  return -1;
}

int sys_munmap(void) {
  uint addr;
  int length;
  
  if (argint(0, (int*) &addr) < 0 || argint(1, &length) < 0)
    goto failure;

  if (length <= 0 || addr < MMAP_BASE || addr >= KERNBASE)
    goto failure;

  struct mmap* mp;
  struct proc* p = myproc();

  // Find the mmap region that uses the addr
  for (mp = p->mmaps; mp < &p->mmaps[N_MMAPS]; mp++)
    if (mp->is_valid && mp->start_addr <= addr && mp->end_addr > addr)
      goto found_mmap;

  // No mmap found at `addr`, failure
  goto failure;

  found_mmap:
  // Is the mapping was ANONYMOUS or PRIVATE, we do not write to a file
  if (IS_MMAP_ANON(mp->flags) || IS_MMAP_PRIVATE(mp->flags))
    goto free_mmap;

  if (!mp->f)
    goto failure;

  // Fileread updates the file offset.
  // We set offset back to 0, to write to the start of the file.
  uint temp = mp->f->off;
  mp->f->off = 0;

  if(filewrite(mp->f, (char*) mp->start_addr, mp->length) != mp->length)
    return -1;

  // Reset offset, process has the illusion nothing was ever changed
  mp->f->off = temp;

  fileclose(mp->f);

  free_mmap:
  // calculate length to deallocate
  addr = PGROUNDDOWN(mp->start_addr);
  uint end = PGROUNDUP(addr + mp->length);

  pte_t* pt_entry;
  // Find page table entry for every mmaped page
  // and free the physical pages.
  for(; addr < end; addr += PGSIZE) {
    if ((pt_entry = walkpgdir(p->pgdir, (char*) addr, 0)) == 0)
      continue;

    kfree(P2V(PTE_ADDR(*pt_entry)));

    // Invalidate the page table entry
    *pt_entry = 0;
  }

  //////////////////////////////     KNOWN BUG     /////////////////////////////
  // IDEALLY SHOULD ONLY MEMSET TO 0g IF TOTAL MMAP IS REMOVED
  memset(mp, 0, sizeof(struct mmap));
  //////////////////////////////   END KNOWN BUG   /////////////////////////////
  // success:
  return 0;

  failure:
  return -1;
}
