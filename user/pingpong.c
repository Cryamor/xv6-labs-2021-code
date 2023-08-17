#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int parent_to_child[2], child_to_parent[2]; // [0]为读端，[1]为写端
    char msg;

    if(pipe(parent_to_child) < 0 || pipe(child_to_parent) < 0){
        fprintf(2, "Failed to create pipes\n");
        exit(1);
    }

    int pid = fork();
    if(pid == 0){
        // 子进程
        // 关闭不用的端
        close(child_to_parent[0]); 
        close(parent_to_child[1]); 
        // 子进程接收父进程消息
        if(read(parent_to_child[0], &msg, 1) != 1){
            fprintf(2, "Read Error in child process\n");
            exit(1);
        }
        fprintf(1, "%d: received ping\n", getpid());
        // 子进程向父进程写
        if(write(child_to_parent[1], &msg, 1) != 1){
            fprintf(2, "Write Error in child process\n");
            exit(1);
        }
        //关闭用完的端
        close(child_to_parent[1]); 
        close(parent_to_child[0]); 
        exit(0);
    }
    else{
        // 父进程
        msg = 'A';
        // 关闭不用的端
        close(child_to_parent[1]); 
        close(parent_to_child[0]); 
        // 父进程向子进程写
        if(write(parent_to_child[1], &msg, 1) != 1){
            fprintf(2, "Write Error in parent process\n");
            exit(1);            
        }
        // 父进程接收子进程消息
        if(read(child_to_parent[0], &msg, 1) != 1){
            fprintf(2, "Read Error in parent process\n");
            exit(1);
        }
        fprintf(1, "%d: received pong\n", getpid());
        //关闭用完的端
        close(child_to_parent[0]); 
        close(parent_to_child[1]); 
        exit(0);
    }
    exit(0);
}