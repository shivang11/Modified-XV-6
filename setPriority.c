#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argc, char *argv[])
{
  int pid, new_priority;
  if(argc != 3)
  {
    printf(1,"Invalid number of commands\n");
  }
  else
  {
    pid = atoi(argv[2]);
    new_priority = atoi(argv[1]);

    if(new_priority < 0 || new_priority > 100)
    {
      printf(1,"Priority should be between 0 and 100\n");
    }
    else
    {
        printf(1,"efimeri\n");
        int val=set_priority(pid,new_priority);
        if(val==-1)
        printf(1,"no such process for this pid\n");
        else
        printf(1,"%d\n",val);
    }
  }
  return 0;
}
