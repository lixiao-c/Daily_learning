#define _GNU_SOURCE
#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/mman.h>
#define LEN 1024*8

int main()
{
    int fd;
    fd=open("write_file",O_RDWR);
    char* addr;
    addr=mmap(NULL,LEN,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(addr<0){
        printf("error\n");
        return;
    }
    printf("map addr %x \n",addr);
    while(1){
        if(*(addr)=='c'){
            printf("get message from another process\n");
            return;
        }
        printf("addr value %x \n",*(addr));
    }
}

