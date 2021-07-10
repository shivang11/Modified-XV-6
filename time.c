#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argc, char *argv[])
{  
  int wtime, rtime;
  int pid = fork();

  if(pid == 0)
  {
    int i;  
    for(i = 0; i < argc-1; i++)
    {
      argv[i] = argv[i+1];
    }
    argv[i] = 0;
    exec(argv[0], argv);
  }
  else
  {
    waitx(&wtime, &rtime);
    printf(1, "run_time = %d, wait_time = %d\n", rtime, wtime);
  }
  return 0;
}
