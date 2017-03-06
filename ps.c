#include "types.h"
#include "user.h"
#include "uproc.h"


int
main(int argc, char *argv[])
{
	int max = 16;

	struct uproc * u = malloc(max * sizeof(struct uproc));

	int num = getprocs(max, u);

	if(num < 0)
		printf(1, "Error, try again.");
	else
	{

		printf(1,"PID 	Name 	UID 	GID 	Parent ID 	Prio 	Elapsed   CPU	State 	Size\n");

		for(int i = 0; i < num; i++)
		{
			printf(1,"%d 	%s 	%d 	%d 	%d 		%d	%d.%d%d 	  %d.%d%d 	%s 	%d\n", u[i].pid, u[i].name, u[i].uid, u[i].gid, u[i].ppid, u[i].prio, (u[i].elapsed_ticks/100), ((u[i].elapsed_ticks%100)/10), (u[i].elapsed_ticks%10), (u[i].CPU_total_ticks/100), ((u[i].CPU_total_ticks%100)/10), (u[i].CPU_total_ticks%10), u[i].state, u[i].size);
		}
	}

	exit();
}
	