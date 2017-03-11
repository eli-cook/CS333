#include "types.h"
#include "user.h"

int main(int argc, char ** argv) {

#ifdef CS333_P5

	uint uid;
	char * filepath;

	if(argc != 3) {
		printf(1,"Invalid arguments\n Usage: chown [uid] [filepath]\n");
		exit();
	}
	else {

		if (argv[1][0] == '-') {
			printf(1,"Invalid arguments: uid must be a positive number.\n");
			exit();
		}
		
		filepath = argv[2];
		uid = atoi(argv[1]);
	}

	if(chown(filepath, uid) < 0) {
		printf(1,"chown was unsuccessful.\n");
	}

#else
	printf(1,"File protection isn't implemented, turn on P5 flag.\n");
#endif

	exit();

}