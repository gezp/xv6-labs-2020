#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXARG  32


int
main(int argc, char *argv[])
{
  int i;
  if(argc <= 1){
    fprintf(2, "Error:argc <=1 \n");
    exit(1);
  }
  char *my_argv[MAXARG];
  for(i=0;i<MAXARG;i++){
    my_argv[i]=0;
  }
  //set argv
  for(i=0;i<argc-1;i++){
    my_argv[i]=argv[i+1];
  }
  char buf[512];
  int len=0;
  int pid;
  while(1){
    //read a line
    len=0;
    while (1) {
        if (read(0,&buf[len],1) == 0 || buf[len] == '\n') break;
        len++;
    }
    if(len==0){
      //finish
      break;
    }
    pid=fork();
    if(pid>0){
      wait(0);
    }else if(pid == 0){
      //process a line
      buf[len]='\0';
      my_argv[argc-1]=buf;
      //exec
      exec(my_argv[0], my_argv);
      exit(0);
    }else{
      printf("Error: failed to fork!\n");
    }
  }
  exit(0);
}
