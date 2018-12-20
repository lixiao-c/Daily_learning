#define _GNU_SOURCE
#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
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
    printf("mmap addr %x \n",addr);
   *(addr)='c';
    if(*(addr))
        printf("write addr\n");
    return 0;
}
