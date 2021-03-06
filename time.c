#include "types.h"
#include "user.h"


/**
 * [Eli] The time command is ran with a console command as its argument. Using a fork(), then exec() to launch the inputted console command,
 * after setting the start time, it waits for the command to finish. The function then sets the end time and prints
 * the time it took the console command to run.
 */
int time(char * argv[])
{
	int time_in;
	int time_out;
	int time_total;
	int pid = getpid();
	char* name = argv[0];
	int check = 0;

	time_in = uptime();
	fork();
	// Child branch from fork. Run inputted console command.
	if(pid != getpid())
	{
		exec(argv[0], &argv[0]);
		printf(1, "Not a valid command");
		exit();
	}	
	// Parent branch from fork. Wait for child process to finish.
	if(pid == getpid() && check == 0)
	{
		wait();
		time_out = uptime();
		time_total = time_out - time_in;
		printf(1, "\n");
		printf(1, "%s ", name);
		printf(1,"ran in %d.%d%d seconds \n", time_total/100, time_total % 100 / 10, time_total%10);
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	int default_time = 0;
	if(argv[1] == 0)
		printf(1, "\nran in %d.%d%d seconds \n",default_time,default_time,default_time);
	else
		time(&argv[1]);

	exit();
}