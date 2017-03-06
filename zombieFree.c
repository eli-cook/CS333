#include "types.h"
#include "user.h"

#define PrioCount 7
#define numChildren 5

/**
 * [Eli] ZombieFree is used for testing functionality of new scheduler management functionality.
 * By pairing this function with CTL commands, multiple requirements in the Project 3 pdf can be met.
 * ZombieFree() forks the max number of times and puts children to sleep. Then kills them in order slowly.
 * This demonstrates processes moving through their entire lifecyle and returning to the unused list. 
 */
void zombieFree() {

 	int n, ppid = getpid(), pid[80];

 	// Forking max number of times
	for(n=0; n<80; n++){
		pid[n] = fork();

		if(pid[n] < 0)
			break;
		if(pid[n] == 0) {
			//printf(1,"I am pid %d\n", getpid());
			sleep(1000000);
			exit();
		}
	}
	
	if (getpid() == ppid) {
		printf(1, "Forked %d times\n", n);
		printf(1, "Press CTL commands to see current state after forking\n");
		sleep(1000);

		for (int m = 0; m < n; m++) {
			kill(pid[m]);
			sleep(100);
			while(wait() == -1){};
			printf(1, "killed pid: %d\n", pid[m]);
		}
	}
}

void showRunnableRR() {

	int n, ppid;
	ppid = getpid();

/*	for(n=0; n<3; n++){
		pid = fork();
		if(n == 2)
	    	for(;;);
	    if(pid == 0)
	      wait();
	}*/

	for (n = 0; n < 6; n++) {
		if(getpid() == ppid)
			fork();
		else
			for(;;);
	}

	sleep(200);

	if(ppid == getpid()) {

		//used for generic testing of set prio and mlfq functionality
		/*printf(1,"moving kids to prio 6");

		for (int i = ppid + 1; i < ppid + 7; i++) {
			setpriority(i, 6);
		}

		sleep(500);

		//used for generic testing of set prio and mlfq functionality
		printf(1,"moving kids to prio 0");

		for (int i = ppid+1; i < ppid + 7; i++) {
			setpriority(i, 0);
		}*/
		//used to test error handling of set priority
		printf(1, "\nattempting to set priority of pid: %d to 8, which is not a valid prio.\n", ppid+1);
		sleep(200);
		if(setpriority(ppid+1, 8) == -1)
			printf(1, "\ncannot set priority for pid: %d\n", ppid+1);

		printf(1, "\n Next test");
		sleep(200);

		printf(1, "\nattempting to set priority of invalid pid: %d to 0.\n", 50);
		sleep(200);
		if(setpriority(50, 0) == -1)
			printf(1, "\ncannot set priority for pid: %d\n", 50);
	}

	sleep(3000);

	return;	
}

void showSleep() {
	//for(;;);

	sleep(10000);
}

void countForever(int p) {


  int j;
  unsigned long count = 0;

  j = getpid();
  p = p%PrioCount;
  setpriority(j, p);
  printf(1, "%d: start prio %d\n", j, p);

  while (1) {
    count++;
    if ((count & 0xFFFFFFF) == 0) {
      p = (p+1) % PrioCount;
      setpriority(j, p);
      printf(1, "%d: new prio %d\n", j, p);
    }
  }
}


int
main(int argc, char *argv[])
{
  	//zombieFree();
	showRunnableRR();
	//for testing set prio
	//int i, rc;

  /*for (i=0; i<numChildren; i++) {
    rc = fork();
    if (!rc) { // child
      countForever(i);
    }
  }*/
  // what the heck, let's have the parent waste time as well!
  //countForever(6);
  
  printf(1,"Successfully forked and killed max number of procs!\n");
  exit();
}