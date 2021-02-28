#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;

  if(argc <= 1){
    fprintf(2, "usage: sleep [time]\n");
    exit(1);
  }
  i=atoi(argv[1]);
  sleep(i);
  exit(0);
}
