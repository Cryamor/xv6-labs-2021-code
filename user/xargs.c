#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[])
{
    if(argc < 2){
        fprintf(2, "Usage: xargs <command> [args...]\n");
        exit(1);
    }

    char* args[MAXARG], buf[1024];
    char c;
    int pid, i, j = 0, status = 1;

    for(i = 0; i < MAXARG - 1 && i < argc - 1; i++){
        args[i] = argv[i + 1];
    }
    args[i] = 0; // 最后一个参数为NULL，标记参数列表的结束，传递给 exec 不会出错

    while(status && i < MAXARG - 1){
        i = argc - 1; // 标记第几行
        args[i++] = &buf[j]; //第一行的起始位置
        while(1){ // 读一行输入
            if((status = read(0, &c, 1)) == 0)
                exit(0); // 从标准输入中读取，没有数据时退出
            if(c == '\n' || c == ' '){
                buf[j++] = 0;
                args[i++] = &buf[j];
                if(c == '\n')
                    break;
            }
            else{
                buf[j++] = c;
            }
        }
        args[i] = 0;
        
        if((pid = fork()) < 0){
            fprintf(2, "xargs: cannot fork\n");
            exit(1);
        }
        else if(pid == 0)
            // 子进程
            exec(argv[1], args);
        else
            // 父进程
            wait(0);
    }

    exit(0);
}