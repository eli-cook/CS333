#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "uproc.h"
#include "spinlock.h"

/** 
 * [Eli] Structure for keeping track of processes using linked lists 
 */
struct StateLists {
  struct proc* runnable[MAX];
  struct proc* unused;
  struct proc* sleep;
  struct proc* zombie;
  struct proc* running;
  struct proc* embryo;
};

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  struct StateLists pLists;
  uint PromoteAtTime;
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  #ifndef CS333_P3P4
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);

  // [Eli] Attempts to remove from head of unused list then uses goto to jump to state transition. Allowed use of goto.
  #else
  acquire(&ptable.lock);
  if((p = removefromhead(&ptable.pLists.unused, UNUSED)) != 0)
    goto found;

  release(&ptable.lock);
  #endif
  return 0;

found:
  p->state = EMBRYO;

  #ifdef CS333_P3P4
  addtohead(p, &ptable.pLists.embryo, EMBRYO);
  #endif

  p->pid = nextpid++;
  p->prio = 0;
  p->budget = BUDGET;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    acquire(&ptable.lock);

    // [Eli] Put process back onto unused from embryo list if process can't allocate space.
    #ifdef CS333_P3P4
    remove(p, &ptable.pLists.embryo, EMBRYO);
    p->state = UNUSED;
    addtohead(p, &ptable.pLists.unused, UNUSED);
    #else
    p->state = UNUSED;
    #endif

    release(&ptable.lock);
    return 0;
  }
  
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  p->start_ticks = ticks;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  acquire(&ptable.lock);

  // [Eli] Initializes all the pointers to the heads of the linked lists
  // and initializes all processes in process array to unused.

  ptable.pLists.embryo = 0;
  for(int i = 0; i < MAX; i++)
    ptable.pLists.runnable[i] = 0;
  ptable.pLists.sleep = 0;
  ptable.pLists.zombie = 0;
  ptable.pLists.running = 0;

  ptable.pLists.unused = ptable.proc;

  for(int i = 0; i < NPROC - 1; i++) {
    ptable.proc[i].next = &ptable.proc[i + 1];
  }

  ptable.proc[NPROC - 1].next = 0;

  release(&ptable.lock);
  
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  acquire(&ptable.lock);

  // [Eli] Add process to runnable from embryo list.
  #ifdef CS333_P3P4
  remove(p, &ptable.pLists.embryo, EMBRYO);
  p->state = RUNNABLE;
  addtotail(p, &ptable.pLists.runnable[0], RUNNABLE);
  #else
  p->state = RUNNABLE;
  #endif

  release(&ptable.lock);
  p->gid = DEFAULT_GID;
  p->uid = DEFAULT_UID;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;

    acquire(&ptable.lock);

    // [Eli] Add process to unused from embryo list.
    #ifdef CS333_P3P4
    remove(np, &ptable.pLists.embryo, EMBRYO);
    np->state = UNUSED;
    addtohead(np, &ptable.pLists.unused, UNUSED);
    #else
    np->state = UNUSED;
    #endif

    release(&ptable.lock);

    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;
  np->uid = proc->uid;
  np->gid = proc->gid;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));
 
  pid = np->pid;


  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);

  // [Eli] Add process to runnable from embryo list.
  #ifdef CS333_P3P4
  remove(np, &ptable.pLists.embryo, EMBRYO);
  np->state = RUNNABLE;
  addtotail(np, &ptable.pLists.runnable[0], RUNNABLE);
  #else
  np->state = RUNNABLE;
  #endif

  release(&ptable.lock);
  
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifndef CS333_P3P4
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

#else

void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.

  // [Eli] Add process to zombie from running list.
  remove(proc, &ptable.pLists.running, RUNNING);
  proc->state = ZOMBIE;
  addtohead(proc, &ptable.pLists.zombie, ZOMBIE);

  sched();
  panic("zombie exit");
}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifndef CS333_P3P4
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

#else

int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);

        // [Eli] Add process to unused from zombie list.
        remove(p, &ptable.pLists.zombie, ZOMBIE);
        p->state = UNUSED;
        addtohead(p, &ptable.pLists.unused, UNUSED);

        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifndef CS333_P3P4
// original xv6 scheduler. Use if CS333_P3P4 NOT defined.
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->cpu_ticks_in = ticks;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}

#else
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    if(ticks >= ptable.PromoteAtTime)
    {
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
      promoterunnable();
    }

    // [Eli] Attempt to remove from runnable, if able to, add process to running list.
    for(int i = 0; i < MAX; i++) {
      if((p = removefromhead(&ptable.pLists.runnable[i], RUNNABLE)) != 0)
        break;
    }
    if(p) {

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;
      switchuvm(p);
      p->state = RUNNING;

      addtohead(p, &ptable.pLists.running, RUNNING);

      p->cpu_ticks_in = ticks;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
#ifndef CS333_P3P4
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  proc->cpu_ticks_total += ticks - proc->cpu_ticks_in;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// [Eli] No changes in sched(), will be making changes to it in Project 4
#else
void
sched(void)
{

  int intena;
  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  proc->cpu_ticks_total += ticks - proc->cpu_ticks_in;
  proc->budget = proc->budget - (ticks - proc->cpu_ticks_in);

  if(proc->budget <= 0 && proc->prio != (MAX - 1)) {
    if(proc->state == RUNNABLE) {
      remove(proc, &ptable.pLists.runnable[proc->prio], RUNNABLE);
      proc->prio++;
      addtotail(proc, &ptable.pLists.runnable[proc->prio], RUNNABLE);
    }
    else
      proc->prio++;
    proc->budget = BUDGET;
  }

  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}
#endif

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  
  // [Eli] Add process to runnable from running list.
  #ifdef CS333_P3P4
  remove(proc, &ptable.pLists.running, RUNNING);
  proc->state = RUNNABLE;
  addtotail(proc, &ptable.pLists.runnable[proc->prio], RUNNABLE);
  #else
  proc->state = RUNNABLE;
  #endif

  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// 2016/12/28: ticklock removed from xv6. sleep() changed to
// accept a NULL lock to accommodate.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){
    acquire(&ptable.lock);
    if (lk) release(lk);
  }

  // Go to sleep.
  proc->chan = chan;

  // [Eli] Add process to sleeping from running list.
  #ifdef CS333_P3P4
  remove(proc, &ptable.pLists.running, RUNNING);
  proc->state = SLEEPING;
  addtohead(proc, &ptable.pLists.sleep, SLEEPING);
  #else
  proc->state = SLEEPING;
  #endif

  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){ 
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

//PAGEBREAK!
#ifndef CS333_P3P4
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
    }
}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan) {

      // [Eli] Add process to runnable from sleeping list. There is no need to obtain and
      // release lock since this is handled by the wrapper function, wakeup().
      remove(p, &ptable.pLists.sleep, SLEEPING);
      p->state = RUNNABLE;
      addtotail(p, &ptable.pLists.runnable[p->prio], RUNNABLE);
    }

}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifndef CS333_P3P4
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING) {

        // [Eli] Add process to runnable from sleeping list.
        remove(p, &ptable.pLists.sleep, SLEEPING);
        p->state = RUNNABLE;
        addtotail(p, &ptable.pLists.runnable[p->prio], RUNNABLE);
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif

static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
};

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  int i;
  int ppid = 1;
  struct proc *p;
  char *state;
  uint pc[10];

  cprintf("\nPID	Name 	UID 	GID	PPID Prio  CPU 	Elapsed State	Size 		PCs\n");
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    if(p->pid != 1)
    	ppid = p->parent->pid;
		cprintf("%d 	%s 	%d 	%d  	%d    %d	   %d.%d%d %d.%d%d 	%s 	%d", p->pid, p->name, p->uid, p->gid, ppid, p->prio, (p->cpu_ticks_total/100), ((p->cpu_ticks_total % 100)/10),(p->cpu_ticks_total%10), (ticks - p->start_ticks)/100, ((ticks - p->start_ticks) % 100)/10, (ticks - p->start_ticks) %10, state, p->sz);    
		if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      cprintf("		");
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf("%p", pc[i]);
    }
    cprintf("\n");
  }
}


/**
 * [Eli] Obtains a paired down version of the process array to pass to the user command "ps" 
 */
int
getuproc(uint max, struct uproc * u)
{
	struct proc * p;

	int count = 0;

	acquire(&ptable.lock);

	for(p = ptable.proc; p < &ptable.proc[NPROC] && p < &ptable.proc[max]; p++)
	{
		if(p->state == UNUSED || p->state == EMBRYO)
			continue;
		u->pid = p->pid;
		u->uid = p->uid;
		u->gid = p->gid;
    u->prio = p->prio;
		if(p->pid == 1)
			u->ppid = 1;
		else
			u->ppid = p->parent->pid;
		u->elapsed_ticks = ticks - p->start_ticks;
		u->CPU_total_ticks = p->cpu_ticks_total;
		safestrcpy(u->state, states[p->state], sizeof(u->state)/sizeof(char));
		u->size = p->sz;
		safestrcpy(u->name, p->name, sizeof(u->name)/sizeof(char));
		u++;
		count++;
	}

	release(&ptable.lock);
	return count;
}


/**
 * [Eli] Prints to the console the number of processes in the unused list.
 */
int freedump() {

  acquire(&ptable.lock);
  struct proc * curr = ptable.pLists.unused;

  // Since we check for next pointers and cover the 0 free cases in the next statement this is safe.
  int count = 1; 

  if(!curr)
    return 0;

  while(curr->next)
  {
    count++;
    curr = curr->next;
  }

  cprintf("Number of unused processes is: %d\n", count);

  release(&ptable.lock);

  return 0;
}

/**
 * [Eli] Prints the runnable linked list in the form of "PID -> PID2 -> etc" 
 */
int runnabledump() {

  for(int i = 0; i < MAX; i++) {
    acquire(&ptable.lock);
    struct proc * curr = ptable.pLists.runnable[i];


    if(!curr) {
      cprintf("\n%d: No processes in the runnable list", i);
      release(&ptable.lock);
      continue;
    }

    cprintf("\n%d: Runnable Procs: ", i);
    while(curr->next)
    {
      cprintf("(%d, %d) -> ", curr->pid, curr->budget);
      curr = curr->next;
    }

    cprintf("(%d, %d) ", curr->pid, curr->budget);
    cprintf("\n");
    release(&ptable.lock);
  }
  cprintf("\n");
  return 0;
}

/**
 * [Eli] Prints the sleep linked list in the form of "PID -> PID2 -> etc" 
 */
int sleepdump() {

  acquire(&ptable.lock);
  struct proc * curr = ptable.pLists.sleep;


  if(!curr) {
    cprintf("No processes in the sleep list\n");
    release(&ptable.lock);
    return 0;
  }

  cprintf("Sleeping Procs: ");
  while(curr->next)
  {
    cprintf("%d -> ", curr->pid);
    curr = curr->next;
  }

  cprintf("%d -> ", curr->pid);

  cprintf("\n");
  release(&ptable.lock);

  return 0;
}

/**
 * [Eli] Prints the zombie linked list in the form of "PID, PPID -> PID2, PPID2 -> etc" 
 */
int zombiedump() {

  acquire(&ptable.lock);
  struct proc * curr = ptable.pLists.zombie;


  if(!curr) {
    cprintf("No processes in zombie list\n");
    release(&ptable.lock);
    return 0;
  }

  cprintf("Zombie Procs: ");
  while(curr->next)
  {
    cprintf("(%d, %d) -> ", curr->pid, curr->parent->pid);
    curr = curr->next;
  }

  cprintf("(%d, %d) -> ", curr->pid, curr->parent->pid);

  cprintf("\n");
  release(&ptable.lock);

  return 0;
}


/**
 * [Eli] Takes a pointer to the head pointer and a state enum.
 * If there are no processes to remove, return a null pointer, this functionality is required for the scheduler.
 * If the to be removed process is not of the correct state, panic.
 */
struct proc * removefromhead(struct proc ** list, enum procstate state) {

  struct proc * proc = *list;
  if(!proc) {
    return proc;
  }

  if((*list)->state != state)
  panic("Incorrect state, won't find proc to remove");

  *list = (*list)->next;
  return proc;
}

/** 
 * [Eli] Takes a pointer to a process, a pointer the list's head pointer, and a state enum.
 * If the process to be added has the wrong state, panic. Otherwise, add to the head of the list and update the head pointer.
 */
void addtohead(struct proc * p, struct proc ** list, enum procstate state) {
  if(p->state != state) {
    panic("Incorrect state, cannot add to head");
  }
  p->next = *list;
  (*list) = p;

  return;

}

/**
 * [Eli] Takes a pointer to a process, a pointer to the list's head pointer, and a state enum.
 * If the process to be removed has the wrong state, panic. Otherwise search the list to find the process to remove and remove it.
 * If the to be removed process can't be found, panic.
 */
void remove(struct proc * p, struct proc ** list, enum procstate state) {

  if(p->state != state)
    panic("Incorrect state, won't find proc to remove");

  while((*list) != p && (*list) != 0) {
    list = &(*list)->next;
  }

  if((*list) == p) {
    (*list) = p->next;
    return;
  }
  
  panic("Could not find process to remove!");

  return;
}

/**
 * [Eli] Takes a pointer to a process, a pointer the the list's head, and a state enum.
 * If the process to add has the wrong state, panic. Otherwise, go to the end of the list and add to its tail. 
 */
void addtotail(struct proc * p, struct proc ** list, enum procstate state) {
  if(p->state != state)
    panic("Incorrect state, cannot add to tail");
  
  while((*list))
    list = &(*list)->next;

  (*list) = p;
  p->next = 0;
  
}

void promoterunnable() {

  struct proc * p;

  //Bump all in runnable lists up by 1
  for(int i = 1; i < MAX; i++) {
    while(ptable.pLists.runnable[i]) {
      p = removefromhead(&ptable.pLists.runnable[i], RUNNABLE);
      p->prio--;
      p->budget = BUDGET;
      addtotail(p, &ptable.pLists.runnable[i - 1], RUNNABLE);
    }
  }

  struct proc * curr = ptable.pLists.sleep;
  while (curr) {
    if(curr->prio > 0) {
      curr->prio--;
      curr->budget = BUDGET;
    }
    curr = curr->next;
  }

  curr = ptable.pLists.running;
  while (curr) {
    if(curr->prio > 0) {
      curr->prio--;
      curr->budget = BUDGET;
    }
    curr = curr->next;
  }

  curr = ptable.pLists.runnable[0];
  while(curr) {
    curr->budget = BUDGET;
    curr = curr->next;
  }
}

int setprio(int pid, int prio) {
  acquire(&ptable.lock);
  struct proc * curr;

  curr = ptable.pLists.sleep;
  while(curr) {
    if(curr->pid == pid) {
      curr->prio = prio;
      curr->budget = BUDGET;
      release(&ptable.lock);
      return 0;
    }

    curr = curr->next;
  }

  curr = ptable.pLists.running;
  while(curr) {
    if(curr->pid == pid) {
      curr->prio = prio;
      curr->budget = BUDGET;
      release(&ptable.lock);
      return 0;
    }

    curr = curr->next;
  }

  for(int i = 0; i < MAX; i++) {
    curr = ptable.pLists.runnable[i];
    while(curr) {
      if (curr->pid == pid) {
        remove(curr, &ptable.pLists.runnable[i], RUNNABLE);
        curr->prio = prio;
        curr->budget = BUDGET;
        addtotail(curr, &ptable.pLists.runnable[prio], RUNNABLE);
        release(&ptable.lock);
        return 0;
      }
      curr = curr->next;
    }
  }

  release(&ptable.lock);
  return -1;
}