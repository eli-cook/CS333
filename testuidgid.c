#include "types.h"
#include "user.h"

int 
sys_testuidgid(void)
{
  int uid = 1;
  int gid = 1;
  int ppid;



  //testing uid
  uid = getuid();
  printf(1,"Current UID is : %d\n" , uid);

  //testing fail case
  printf(1,"Attempting to set UID to 32768, should fail --> ");
  setuid(32768);
  uid = getuid();
  printf(1,"Current UID is : %d\n" , uid);

  //testing fail case
  printf(1,"Attempting to set UID to -1, should fail --> ");
  setuid(-1);
  uid = getuid();
  printf(1,"Current UID is : %d\n" , uid);

  printf(1,"Attempting to set UID to 32767 --> ");
  setuid(32767);
  uid = getuid();
  printf(1,"Current UID is : %d\n" , uid);



  //testing gid
  gid = getgid();
  printf(1,"\nCurrent GID is : %d\n" , gid);

  //testing fail case
  printf(1,"Attempting to set GID to 32768, should fail --> ");
  setgid(32768);
  gid = getgid();
  printf(1,"Current GID is : %d\n" , gid);

  //testing fail case
  printf(1,"Attempting to set GID to -1 should fail --> ");
  setgid(-1);
  gid = getgid();
  printf(1,"Current GID is : %d\n" , gid);

  printf(1,"Attempting to set GID to 32767 --> ");
  setgid(32767);
  gid = getgid();
  printf(1,"Current GID is : %d\n" , gid);

  ppid = getppid();
  printf(1,"Parent process is: %d\n" , ppid);
  printf(1,"Done!\n");

  return 0;
}

int
main(int argc, char *argv[])
{
  sys_testuidgid();

  exit();
}

