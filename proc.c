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
void popf(struct proc *t , int );
void pushf(int , struct proc *t);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
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

  p -> start_time = ticks;
  p -> end_time = 0;                              
  p -> run_time = 0;                               
  p -> wait_time = 0;                           
  p -> init_priority = 60;
  p -> queueNo = 0;
  p -> cur_time = 0;
  p -> num_run = 0;
  
  for(int i = 0; i < 5; i++)
  {
    p -> ticks[i] = 0;
  }
  p->state = EMBRYO;
  p->pid = nextpid++;

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
    #ifdef MLFQ
  		pushf(0,p);
	#endif

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
  #ifdef MLFQ
  		pushf(0,np);
	#endif

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
int waitx(int *wtime,int *rtime)
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

        // total run_time
        p->end_time=ticks;
        *rtime = p->run_time;
        // total wait time
        *wtime = (p->end_time-p->start_time)-p->run_time;
         
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
void
scheduler(void)
{
    struct proc *p;
    struct cpu *c = mycpu();
    c->proc = 0;
    
    #ifdef ROUND_ROBIN
    for(;;){
        // Enable interrupts on this processor.
        sti();

        // Loop over process table looking for process to run.
        acquire(&ptable.lock);
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state != RUNNABLE)
            continue;

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        p->num_run++;
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        }
        release(&ptable.lock);

    }
    #endif

    #ifdef FCFS
    
    for(;;)
    {
        sti();
        acquire(&ptable.lock);
        long long int st=123443356789;
        struct proc *temp=0;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        {
            if(p->state!=RUNNABLE)
            continue;
            // find process with minimum creation time

            if(p->start_time<=st)
            {
                st=p->start_time;
                temp=p;
            }
        }
        if(temp!=0)
        {
            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            temp->num_run++;
            c->proc = temp;
            switchuvm(temp);
            temp->state = RUNNING;

            swtch(&(c->scheduler), temp->context);
            switchkvm();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
        }

        release(&ptable.lock);
        
    }
    #endif

    #ifdef PBS
    struct proc * q;
    struct proc * s;
    q=ptable.proc;
    //find highest priority process
    for(;;)
    {
        sti();
        acquire(&ptable.lock);
        long long int st=27237623763487;
        struct proc *temp=0;
        for(p = q; p < &ptable.proc[NPROC]; p++)
        {
            if(p->state!=RUNNABLE)
            continue;

            if(p->init_priority<=st)
            {
                st=p->init_priority;
                temp=p;
            }
        }
        for(s = ptable.proc; s<q; s++)
        {
            if(s->state!=RUNNABLE)
            continue;

            if(s->init_priority<=st)
            {
                st=s->init_priority;
                temp=s;
            }
        }

        if(temp!=0)
        {
            q=temp;
            temp->num_run++;
            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            c->proc = temp;
            switchuvm(temp);
            temp->state = RUNNING;

            swtch(&(c->scheduler), temp->context);
            switchkvm();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
        }

        release(&ptable.lock);
        
    }
    #endif

    #ifdef MLFQ
    
    for(;;)
    {

        sti();
        acquire(&ptable.lock);
        for(int i=1; i < 5; i++)
        {
            if(front[i]==-1)
            continue;

            
            if(front[i]<=rear[i]){
            for(int j=front[i]; j <=rear[i]; j++)
            {
                struct proc *r = queue[i][j];
                if(r->wait_time> AGE)
                {
                //    cprintf("aging\n");
                    popf(r, i);
                    r->queueNo=i-1;
                    r->wait_time=0;
                    r->cur_time=0;
                    pushf(i-1,r);
                }

            }}

            else
            {
                for(int j=0; j <=rear[i]; j++)
                {
                    struct proc *r = queue[i][j];
                    if(r->wait_time> AGE)
                    {
                    //    cprintf("aging\n");
                        popf(r, i);
                        r->queueNo=i-1;
                         r->wait_time=0;
                        r->cur_time=0;
                        pushf(i-1,r);
                    }

                }
                for(int j=front[i]; j < NPROC; j++)
                {
                    struct proc *r = queue[i][j];
                    if(r->wait_time> AGE)
                    {
                    //    cprintf("aging\n");
                        popf(r, i);
                        r->queueNo=i-1;
                         r->wait_time=0;
                        r->cur_time=0;
                        pushf(i-1,r);
                    }

                }

            }
        }
        struct proc *s=0;
        for(int i=0; i < 5; i++)
        {
            if(rear[i] >=0)
            {
                s = queue[i][front[i]];
                popf(s,i);
                break;
            }
        }
        p=s;
        if(p!=0 && p->state==RUNNABLE)
        {
            p->num_run++;
            c->proc = p;
            switchuvm(p);
            p->state = RUNNING;

            swtch(&(c->scheduler), p->context);
            switchkvm();
            c->proc = 0;

            if(p!=0 && p->state == RUNNABLE)
            {
                if(p->change==1)
                {
                    p->change=0;
                    if(p->queueNo != 4)
                        p->queueNo++;
                    p->cur_time=0;
                    p->wait_time=0;
                }                
                pushf(p->queueNo,p);
            }
        }
        release(&ptable.lock);
    }

    #endif 
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
  // change queue number for MLFQ

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
  {

    if(p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
        #ifdef MLFQ
			p->cur_time = 0;
			pushf(p->queueNo,p);
		#endif 
    }
  }
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
      {
        p->state = RUNNABLE;
        #ifdef MLFQ
			pushf(p->queueNo,p);
		#endif
      }
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

void modify_times()
{
  acquire(&ptable.lock);
  struct proc *p;
//   for(p=ptable.proc ; p < &ptable.proc[NPROC];p++)
//   {
//       if(p->pid<=3 || p->pid>=9)
//       continue;

//       if((ticks-code_start)%20==0)
//       {
//           cprintf("%d,%d,%d\n",p->pid,ticks-p->start_time,p->queueNo);
//       }
//   }
	
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
  	if(p -> state == RUNNING)
    {	
      p -> run_time++;
      
      #ifdef MLFQ
      p -> ticks[p -> queueNo]++;
      p -> cur_time++;      
      #endif
    }
  	else if(p->pid!=0)
  	{	
      p->wait_time++;
    }  
  }

  release(&ptable.lock);
}

int set_priority(int a,int b)
{
  acquire(&ptable.lock);
  struct proc *p;
  int value,fl=0;	

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
      if(p->pid==a)
      {
          fl=1;
          value=p->init_priority;
          p->init_priority=b;
          break;
      }
  }
  release(&ptable.lock);
  if(fl==1)
  return value;
  else
  {
      return -1;
  }
}


int PBSpreempt(int a,int b)
{
    acquire(&ptable.lock);
    struct proc* p = 0;
    int fl=0;

    if(b==0)
    {
        // find a process with higher priority for preemption
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) 
        {
            if(p->init_priority<a)
            {
              fl=1;
              break;  
            }
        }
    }
    else
    {
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) 
        {
            if(p->init_priority<=a)
            {
              fl=1;
              break;  
            }
        }
    }
    release(&ptable.lock);
    return fl;
    
}
void pushf(int a,struct proc *t)
{
    if((front[a] == rear[a] + 1) || (front[a] == 0 && rear[a] == NPROC-1))
    {
     //   cprintf("queue is full\n");
        return ;
    } 
    // check if process already present
    for(int i = 0; i < 5; i++)
    {
        if(front[i] > rear[i])
        {
            for(int j = 0; j < NPROC; j++)
            {
                if(j >= front[i] || j <= rear[i])
                {
                    if(queue[i][j] == t)
                    {
                        return;
                    }
                }
            }
        }
        else
        {
            for(int j = front[i]; j <= rear[i] && j >= 0; j++)
            {
                if(queue[i][j] == t)
                {
                    return;
                }
            }
        }
    }
    
    if(front[a] == -1)
    {
        front[a] = 0;
    }

    rear[a]++;
    rear[a]%=NPROC;
    
    queue[a][rear[a]] = t;
}

void popf(struct proc *t,int a)
{
    if(front[a]==-1 || front[a]==rear[a])
    {
        front[a]=-1;
        rear[a]=-1;
        return ;
    }
    int st=-1;
    if(rear[a]>=front[a]){
	for(int i=front[a]; i <= rear[a]; i++)
	{
		if( queue[a][i]!=0 && queue[a][i] -> pid == t->pid)
		{
			st = i;
			break;
		}
	}
    }
    else
    {
        for(int i=0;i<=rear[a];i++)
        {
            if( queue[a][i]!=0 && queue[a][i] -> pid == t->pid)
            {
                st = i;
                break;
            }
        }
        for(int i=front[a];i<=NPROC;i++)
        {
            if( queue[a][i]!=0 && queue[a][i] -> pid == t->pid)
            {
                st = i;
                break;
            }
        }        
    }
	if(st  == -1)
	{
		return ;
	}
    else
    {
        if(front[a]>=rear[a]){
            if(st>=front[a]){
       for(int i = st; i < NPROC; i++)
		queue[a][i] = queue[a][(i+1)%NPROC];

        for(int i=0;i<rear[a];i++)
        queue[a][i] = queue[a][(i+1)%NPROC];}

        else
        {
            for(int i=st;i<rear[a];i++)
            queue[a][i] = queue[a][(i+1)%NPROC];
        }
        }
        else
        {
            for(int i = st; i < rear[a]; i++)
		    queue[a][i] = queue[a][(i+1)%NPROC];
        }
        rear[a]=(rear[a]-1+NPROC)%NPROC;
    }
    return ;
}

int ps_call(void)
{
    cprintf("PID  PRIORITY   State   r_time  wtime  n_run  cur_q  q0  q1  q2  q3  q4\n");
    struct proc *l;
    for(l = ptable.proc; l < &ptable.proc[NPROC]; l++) 
    {
        if(l->pid==0)
        continue;

        cprintf("%d      ",l->pid);
        cprintf("%d      ",l->init_priority);
        
        if(l->state==RUNNING)
        cprintf("RUNNING     ");
        else if(l->state==EMBRYO)
        cprintf("EMBRYO     ");
        else if(l->state==SLEEPING)
        cprintf("SLEEPING     ");
        else if(l->state==RUNNABLE)
        cprintf("RUNNABLE     ");
        else if(l->state==UNUSED)
        cprintf("UNUSED     ");
        else if(l->state==ZOMBIE)
        cprintf("ZOMBIE     ");

        cprintf("%d    ",l->run_time);
        cprintf("%d    ",l->wait_time);
        cprintf("%d    ",l->num_run);
        cprintf("%d      ",l->queueNo);
        cprintf("%d   ",l->ticks[0]);
        cprintf("%d   ",l->ticks[1]);
        cprintf("%d   ",l->ticks[2]);
        cprintf("%d   ",l->ticks[3]);
        cprintf("%d",l->ticks[4]);

        cprintf("\n");
    }
    return 0;
}
// void print_data()
// {
//     modify_times(1);
//     return ;
// }
