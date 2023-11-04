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

int mmap_alloc(pde_t* pgdir, struct mmap* mp) {
  uint adjusted_end = (uint) (mp->end_addr - (IS_MMAP_GROWSUP(mp->flags) ? PGSIZE: 0));

  uint a = (uint) mp->start_addr;
  char *mem;


  for(; a < adjusted_end; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, adjusted_end, (uint) mp->start_addr);
      return -1;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, adjusted_end, (uint) mp->start_addr);
      kfree(mem);
      return -1;
    }
  }

  return 0;
}

int mmap_read(struct mmap* mp) {
  uint start_addr = mp->start_addr;
  uint end_addr = mp->end_addr;

  if (IS_MMAP_GROWSUP(mp->flags)) {
    end_addr -= PGSIZE;
  }
  ////////////////////SAANVI WRITE YOUR CODE HERE///////////



  ////////////////////SAANVI STOP WRITING YOUR CODE HERE///////////
  success:
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

    for (int i = 0; i < N_MMAPS; i++) {
      mp = &(p->mmaps[i]);
      if (mp->is_valid && fault >= mp->start_addr && fault < mp->end_addr) {
        // TODO
        // if (IS_MMAP_GROWSUP())
        if (mmap_alloc(p->pgdir, mp) < 0) {
          cprintf("FAILED MMAP ALLOC!");
        }

        goto mmap_lazy_done;
      }
    }

    cprintf("Segmentation Fault at address %x\n", (void*) fault);

    mmap_lazy_done:
    if (!(IS_MMAP_ANON(mp->flags))) {
      mmap_read(mp);
    }
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
