  MODIFICATION TO XV6 OPERATING SYSTEM
				ASSIGNMENT 5

OVERVIEW
In this assignment we have made various implementations and changes to xv6 operating system.We have implemented new system calls such as waitx,ps etc .Various scheduling algorithms such as FCFS,PBS,MLFQ have also been implemented.

    • For implementing a new system calls changes in following files is required-
      1.user.h
	 2.usys.S
	 3.syscall.h
	 4.syscall.c
	 5.sysproc.c
	 6.defs.h
	 7.proc.c
	 8.proc.h

WAITX SYSCALL
This new system call is added to proc.c function which works similarly as wait function but also returns the wait time and run time of a process.New parameters are added in proc structure such as wait_time,run_time,start_time,end_time . Therefore  waitx system call returns the wait time and run time of a process.User program to test this is time.c and we can test it using time <command>.

SCHEDULER FUNCTIONS

1.FCFS – In this scheduling algorithm whichever process comes first is executed first . It is a non preemptive and in this we iterate over all the processes and check for their start time   and select with least starting time.

2.PBS – In this scheduling algorithm we select process with highest prioirty (with lowest priority number ) and if two processes have same priority then processes are selected in round robin fashion.We also check that if a process is running and a process with higher priority comes then currently running process is preempted and replaced with higher priority process.

3.MLFQ – In this scheduling algorithm we have assigned time_slice for each queue as 1,2 ,4,8,16 for q0,q1,q2,q3,q4 respectively.We select process from highest priority queue which is non-empty . Also to avoid starvation aging is done with age of 20 . After every age time a process moves to a higher priority queue.After completing time slice priority of process is decreased and it is moved in lower priority queues .

We can test these scheduler functions by using SCHEDULER=(FCFS/PBS/MLFQ)
SET_PRIORITY SYSCALL
This new system call is added to proc.c function to change the priority of a process for PBS scheduler .We can change the priority of a process ranging from [0,100] . This system call returns the old priority of a process.We can test this using setPriority   new_piority pid command.  

PS
This new system call is added to proc.c function to extract the information for each process.We can test this using ps command.

BONUS(graph)
Graphs are made for five processes which depicts the queue number of process with the number of ticks.
Observations for different processes:
For the process that are more cpu bound remain in lower queues and with more io bound remain in higher level queues(queues with low prioirty) as they don’t require much cpu time.



