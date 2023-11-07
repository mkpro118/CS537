#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmap.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

static char* SEGFAULT = "Segmentation Fault\n";

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

int alloc_mem(pde_t* pgdir, uint start, uint end) {
  for(uint a = start; a < end; a += PGSIZE){
    char *mem;
    if((mem = kalloc()) == 0){
      cprintf("mmap out of memory\n");
      return -1;
    }

    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*) a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("mmap out of memory (2)\n");
      kfree(mem);
      return -1;
    }
  }
  return 0;
}

int mmap_alloc(pde_t* pgdir, struct mmap* mp) {
  uint adjusted_end = (uint) (mp->end_addr - (IS_MMAP_GROWSUP(mp->flags) ? PGSIZE: 0));

  return alloc_mem(pgdir, (uint) mp->start_addr, adjusted_end);
}

int mmap_read(struct mmap* mp) {
  struct proc* p = myproc();

  if(fileread(mp->f, (char*) mp->start_addr, mp->length) < 0)
    goto fail;

  //success:
  return 0;

  fail:
  return -1;
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  case T_PGFLT:
    uint fault = rcr2(); // gets the fault address
    struct proc* p = myproc();
    struct mmap* mp;

    for (mp = p->mmaps; mp < &p->mmaps[N_MMAPS]; mp++) {
      if (mp->is_valid && fault >= mp->start_addr && fault < mp->end_addr) {
        if (walkpgdir(p->pgdir, (void*) mp->start_addr, 0) == 0)
          goto mmap_lazy_alloc;

        if (!IS_MMAP_GROWSUP(mp->flags))
          break;

        goto alloc_guard;
      }
    }

    goto seg_fault;

    mmap_lazy_alloc:
    if (mmap_alloc(p->pgdir, mp) < 0)
      goto seg_fault;


    if (!(IS_MMAP_ANON(mp->flags)))
      mmap_read(mp);

    goto done_mmap_alloc;

    alloc_guard:
    if (!(fault >= mp->end_addr - PGSIZE))
      goto seg_fault;


    struct mmap* mp2;
    for (mp2 = p->mmaps; mp2 < &p->mmaps[N_MMAPS]; mp2++)
      if (mp2->is_valid && mp2->start_addr == mp->end_addr)
        goto seg_fault;

    fault = PGROUNDDOWN(fault);

    // No mmap already has that, so allocate.
    if (alloc_mem(p->pgdir, fault, fault + PGSIZE) < 0)
      goto seg_fault;

    // GUARD was allocated properly, set new guard
    mp->length += PGSIZE;
    mp->end_addr += PGSIZE;

    goto done_mmap_alloc;

    seg_fault:
    cprintf(SEGFAULT);
    p->killed = 1;

    done_mmap_alloc:
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
