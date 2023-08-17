#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void child(int p[]){
    close(p[1]);
    int prime;
    if(read(p[0], &prime, 4) > 0){
        fprintf(1, "prime %d\n", prime);
        int p1[2]; // 创建管道向右侧进程传递
        pipe(p1);
        if(fork() > 0){
            // 本进程
            close(p1[0]);
            int i;
            while(read(p[0], &i, 4) > 0){
                if(i % prime != 0)
                    write(p1[1], &i, 4);
            }
            close(p1[1]);
            wait(0);
        }
        else{
            // 右侧进程
            close(p[0]);
            child(p1);
        }
    }
}

int main() 
{
    int p[2];
    pipe(p);
    int pid = fork();
    if(pid < 0){
        fprintf(2, "Fork failed\n");
        exit(1);
    }
    else if(pid > 0){
        // 最左侧进程
        close(p[0]);
        fprintf(1, "prime 2\n");
        for(int i = 3; i <= 35; i++){
            if(i % 2 != 0)
                write(p[1], &i, 4);
        }
        close(p[1]);
        wait(0);
    }
    else
        child(p);
    
    exit(0);
}