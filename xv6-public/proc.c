#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

struct mlfq mq;

int global_tick = 0;


// MLFQ FUNCTIONS
// init mlfq. set each queue's front rear 0
void
init_queue(struct mlfq *mq)
{
  for(int i = 0; i < 3; i++) {
    for(int j = 0; j < 64; j++) {
      mq -> arr[i][j] = 0;
    }
    mq -> front[i] = 0;
    mq -> rear[i] = 0;
  }
}


// check the emptiness of the queue
int
isEmpty(struct mlfq *mq, int level)
{
  if(mq -> front[level] == mq -> rear[level])
    return 1;
  return 0;
}

// check whether the queue is full or not
int
isFull(struct mlfq *mq, int level)
{
  if(mq -> front[level] == (mq -> rear[level] + 1) % 64)
    return 1;
  return 0;
}

// enqueue the process into the queue
void
enqueue(struct mlfq *mq, struct proc *p, int level)
{
  if(isFull(mq, level)) {
    panic("queue is full\n");
  } else {
    mq -> rear[level] = (mq -> rear[level] + 1) % 64;
    mq -> arr[level][mq -> rear[level]] = p;
    p -> queue_idx = mq -> rear[level];
  }  
}

// dequeue the process and return it
struct proc*
dequeue(struct mlfq *mq, int level)
{
  struct proc *p;
  if(isEmpty(mq, level)) {
    panic("empty queue\n");
    return 0;
  } else {    
    // int pos = (mq -> front[level] + 1) % 64;
    // p = mq -> arr[level][pos];
    // mq -> arr[level][pos] = 0;
    // mq -> front[level] = pos;
    mq -> front[level] = (mq -> front[level] + 1) % 64;
    p = mq -> arr[level][mq -> front[level]];
    mq -> arr[level][mq -> front[level]] = 0;
    return p;
  }
}


int
get_queue_size(struct mlfq *mq, int level) {
  return (mq -> rear[level] - mq -> front[level] + 64) % 64;
}

// delete the process from the queue
void
delete_queue(struct mlfq *mq, int idx, int level)
{
  int x = idx - mq -> front[level] + 64;
  x %= 64;
  for(int i = 1; i < x; i++) {
    struct proc *temp = dequeue(mq, level);
    enqueue(mq, temp, level);
  }
  dequeue(mq, level);
}



// get minimum priority of l2. this can be used only for l2
int
getMinPriority(struct mlfq *mq) 
{
  if(isEmpty(mq, 2)) {
    panic("empty l2 queue\n");
    return -1;
  } else {
    int min = 3;
    for(int i = (mq -> front[2] + 1) % 64; i <= mq -> rear[2]; i = (i + 1) % 64) {
      if(mq -> arr[2][i] -> priority < min)
        min = mq -> arr[2][i] -> priority;
    }
    return min;
  }
  // struct proc *p;
  // int min = 3;
  // for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
  //   if(p -> state == RUNNABLE && p -> priority < min)
  //     min = p -> priority;
  // }
  // return min;
}

void
priorityBoost(struct mlfq *mq) {
  
  // for(int i = 0; i < 3; i++) {
  //   // int x = mq -> rear[i] - mq -> front[i] + 64;
  //   // x %= 64;
  //   int x = get_queue_size(mq, i);
  //   for(int j = 0; j < x; j++) {
  //     struct proc *temp = dequeue(mq, i);
      
  //     temp -> used_time = 0;
  //     temp -> level = 0;
  //     temp -> priority = 3;
  //     enqueue(mq, temp, 0);
  //   }
  // }
  
struct proc *p;
  init_queue(mq);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p -> state != RUNNABLE)
      continue;
    p -> used_time = 0;
    p -> level = 0;
    p -> priority = 3;
    enqueue(mq, p, 0);
  }

}

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  init_queue(&mq);
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

// syscall
void
setPriority(int p, int i)
{
  struct proc *prc;
  if(holding(&ptable.lock)) {
    for(prc = ptable.proc; prc < &ptable.proc[NPROC]; prc++) {
      if(prc -> pid == p)
        prc -> priority = i;
    }
  } else {
    acquire(&ptable.lock);
    for(prc = ptable.proc; prc < &ptable.proc[NPROC]; prc++) {
      if(prc -> pid == p)
        prc -> priority = i;
    }
    release(&ptable.lock);
  }

  // struct proc *prc = myproc();
  // if(p == prc -> level)
  //   prc -> priority = i;
  // else
  //   panic("wrong process\n");
}

// setPriority wrapper
int
sys_setPriority(void)
{
  int p, i;
  if(argint(0, &p) < 0 || argint(1, &i) < 0)
    return -1;

  setPriority(p, i);
  return 0;
}

int
getLevel(void)
{
  // struct proc *p;
  // int lev = -1;
  // if(holding(&ptable.lock)) {
  //   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
  //     if(p -> state == RUNNING) {
  //       lev = p -> level;
  //       break;
  //     }
  //   }
  // } else {
  //   acquire(&ptable.lock);
  //   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
  //     if(p -> state == RUNNING) {
  //       lev = p -> level;
  //       break;
  //     } 
  //   }
  //   release(&ptable.lock);
  // }
  // return lev;

  struct proc *p = myproc();
  if(p == 0)
    return -1;
  
  return p -> level;
}

int
sys_getLevel(void)
{
  return getLevel();
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

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  p -> priority = 3;
  p -> used_time = 0;
  p -> level = 0;
  p -> queue_idx = 0;

  enqueue(&mq, p, p -> level);

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
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

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

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

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
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
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  // delete_queue(&mq, curproc -> queue_idx, curproc -> level);
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;

        // delete process from queue
        // delete_queue(&mq, p -> queue_idx, p -> level);

        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

// void
// scheduler(void)
// {
//   struct proc *p;
//   struct cpu *c = mycpu();
//   c->proc = 0;
  
//   for(;;){
//     // Enable interrupts on this processor.
//     sti();

//     // Loop over process table looking for process to run.
//     acquire(&ptable.lock);
//     for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//       if(p->state != RUNNABLE)
//         continue;

//       // Switch to chosen process.  It is the process's job
//       // to release ptable.lock and then reacquire it
//       // before jumping back to us.
//       c->proc = p;
//       switchuvm(p);
//       p->state = RUNNING;

//       swtch(&(c->scheduler), p->context);
//       switchkvm();

//       // Process is done running for now.
//       // It should have changed its p->state before coming back.
//       c->proc = 0;
//     }
//     release(&ptable.lock);

//   }
// }

void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c -> proc = 0;

  for(;;) {
    int found = 0;
    int pos = 0;
    sti();

    acquire(&ptable.lock);
    
    // cprintf("global tick: %d\n", global_tick);


    if(global_tick == 99) {
      // cprintf("priority boost\n");
      priorityBoost(&mq);
      global_tick++;
      global_tick %= 100;
      
      release(&ptable.lock);
      continue;
    }
    global_tick++;
    global_tick %= 100;

    // l0 queue
    for(int i = (mq.front[0] + 1) % 64; i <= mq.rear[0]; i = (i + 1) % 64) {
      p = mq.arr[0][i];
      if(p == 0 && !isEmpty(&mq, 0)) {
        dequeue(&mq, 0);
      } else if(p -> state == RUNNABLE){
        found = 1;
        pos = i;
        break;
      } 
    }

    // if there is no runnable process in l0 queue
    if(found == 0) {
      // l1 queue
      for(int i = (mq.front[1] + 1) % 64; i <= mq.rear[1]; i = (i + 1) % 64) {
        p = mq.arr[1][i];
        if(p == 0 && !isEmpty(&mq, 1)) {
          dequeue(&mq, 1);
        } else if(p -> state == RUNNABLE) {
          found = 1;
          pos = i;
          break;
        }   
      } 
    }
    
    // if there is no runnable process in l1 queue and l2 is not empty
    if(found == 0 && !isEmpty(&mq, 2)) {
      // l2 queue
      int min_priority = getMinPriority(&mq);
      // cprintf("min_priority: %d\n", min_priority);
      for(int i = (mq.front[2] + 1) % 64; i <= mq.rear[2]; i = (i + 1) % 64) {
        p = mq.arr[2][i];
        if(p == 0) {
          dequeue(&mq, 2);
        } else if(p -> priority == min_priority && p -> state == RUNNABLE) {
          found = 1;
          pos = i;
          break;
        }
      }
    }

    // there is no runnable process in mlfq
    if(found == 0) {
      release(&ptable.lock);
      continue;
    }

    // cprintf("queue size: %d, pid: %d, priority: %d, level: %d, used_time: %d, queue_idx: %d, global tick : %d\n", get_queue_size(&mq, p->level), p -> pid, p -> priority, p -> level, p -> used_time, p -> queue_idx, global_tick);
    
    // priority scheduling for l2. find process having highest priority
    if(p -> level == 2) {
      // how many iterations
      int x = pos - mq.front[2] + 64;
      x %= 64;

      // moving processes not to break queue structure
      for(int i = 1; i < x; i++) {
        struct proc *temp = dequeue(&mq, 2);
        enqueue(&mq, temp, 2);
      }
      // now highest priority process is at the front
    }

    c -> proc = p;
    switchuvm(p);
    p -> state = RUNNING;

    swtch(&(c -> scheduler), p -> context);
    switchkvm();
    

    p -> used_time++;

    dequeue(&mq, p -> level);
    if(p -> used_time >= (2 * p -> level) + 4) {
      p -> used_time = 0;
      if(p -> level == 2) {
        if(p -> priority > 0) 
          p -> priority -= 1;
        else
          p ->priority = 0;
      }  
      else 
        p -> level += 1;
    }
    enqueue(&mq, p, p -> level);
    c -> proc = 0;
    
    release(&ptable.lock);
  }
}


// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// yield wrap function
int
sys_yield(void)
{
  if(myproc() != 0) {
    yield();
    return 0;
  }
    
  return -1;
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
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

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

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

