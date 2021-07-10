#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argc, char *argv[]) 
{
    if(argc>1)
    {
        printf(1,"invalid number of commands\n");
        return 0;
    }
    
    else
    {
        ps_call();
    }
    return 0;
    
}