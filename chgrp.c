#include "types.h"
#include "user.h"

int main(int argc, char ** argv) {

#ifdef CS333_P5

	uint gid;
	char * filepath;

	if(argc != 3) {
		printf(1,"Invalid arguments\n Usage: chgrp [gid] [filepath]\n");
		exit();
	}
	else {

		if (argv[1][0] == '-') {
			printf(1,"Invalid arguments: gid must be a positive number.\n");
			exit();
		}
		
		filepath = argv[2];
		gid = atoi(argv[1]);
	}

	if(chgrp(filepath, gid) < 0) {
		printf(1,"chgrp was unsuccessful.\n");
	}
#else
	printf(1,"File protection isn't implemented, turn on P5 flag.\n");
#endif

	exit();

}