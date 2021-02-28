#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char* dirpath, char* filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    if ((fd = open(dirpath, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", dirpath);
        return;
    }
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", dirpath);
        close(fd);
        return;
    }
    if (st.type == T_DIR) {
        if (strlen(dirpath) + 1 + DIRSIZ + 1 > sizeof buf) {
            printf("path too long\n");
            return;
        }
        strcpy(buf, dirpath);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (stat(buf, &st) < 0) {
                printf("find:cannot stat %s\n", buf);
                continue;
            }
            //check item type and name in directory
            if(st.type== T_FILE){
                if(strcmp(de.name,filename)==0){
                    printf("%s\n",buf);
                }
            }else if(st.type == T_DIR){
                if(strcmp(de.name,".")!=0 && strcmp(de.name,"..")!=0){
                    //recursive 
                    find(buf,filename);
                } 
            }
        }
    }
    close(fd);
}

int main(int argc, char* argv[])
{
    if (argc <= 2) {
        fprintf(2, "usage: find dir filename\n");
        exit(0);
    }
    find(argv[1],argv[2]);
    exit(0);
}
