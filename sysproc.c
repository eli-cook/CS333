#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "uproc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      return -1;
    }
    sleep(&ticks, (struct spinlock *)0);
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since start. 
int
sys_uptime(void)
{
  uint xticks;
  
  xticks = ticks;
  return xticks;
}

//Turn of the computer
int sys_halt(void){
  cprintf("Shutting down ...\n");
// outw (0xB004, 0x0 | 0x2000);  // changed in newest version of QEMU
  outw( 0x604, 0x0 | 0x2000);
  return 0;
}

//student added
int
sys_date(void) //P1
{

  struct rtcdate *d;

  if(argptr(0, (void*) &d, sizeof(*d)) < 0)
    return -1;
  cmostime(d);
  return 0;
}

int 
sys_getuid(void) //P2
{
  return proc->uid;
}


int
sys_getgid(void) //P2
{
  return proc->gid;
}

int
sys_getppid(void) //P2
{
  if(proc->pid == 1)
    return 1;
  else
    return proc->parent->pid;
}

int 
sys_setuid(void) //P2
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  if(n > 32767 || n < 0)
    return -1;
  proc->uid = n;
  return 0;
}

int
sys_setgid(void) //P2
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  if(n > 32767 || n < 0)
    return -1;
  proc->gid = n;
  return 0;
}

int sys_getprocs(void)
{
  int max;
  struct uproc * u;
  if(argint(0, &max) < 0)
    return -1;
  if(argptr(1, (void*) &u, sizeof(*u)) < 0)
    return -1;
  return getuproc(max, u);
}
