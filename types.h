typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;

#define AGE 20

void modify_times(void);
int PBSpreempt(int , int);
int ps_call(void);
//void print_data(void);

int front[64];
int rear[64];
int code_start;
int sz[64];
int ticksQ[64];
struct proc* queue[5][64];