#include "types.h"
#include "user.h"



int main(int argc, char ** argv) {

#ifdef CS333_P5

	uint mode;
	char * filepath;
	//int status = 0;

	if(argc != 3) {
		printf(1,"Invalid arguments\n Usage: chmod [mode] [filepath]\n");
		exit();
	}
	else {
		if(strlen(argv[1]) != 4) {
			printf(1,"Invalid arguments: mode must be a 4 digit number.\n");
			exit();
		}

		if (argv[1][0] == '-') {
			printf(1,"Invalid arguments: mode must be a positive number.\n");
			exit();
		}

		if(argv[1][0] != '1' && argv[1][0] != '0') {
			printf(1,"Invalid arguments: mode set id bit must be either a 0 or a 1\n");
			exit();
		}
		
		for(int i = 1; i < 4; i++) {	
			if(argv[1][i] < '0' || argv[1][i] > '7') {
				printf(1,"Invalid arguments: mode permission bits must be between 0 and 7\n");
				exit();
			}
		}

		filepath = argv[2];
	}

  	mode = 0;

	char * num = argv[1];
	
  	while('0' <= *num && *num <= '9')
    	mode = mode*8 + *num++ - '0';

	if(chmod(filepath, mode)) {
		printf(1,"chmod was unsuccessful.\n");
	}

#else
	printf(1,"File protection isn't implemented, turn on P5 flag.\n");
#endif

	exit();

}