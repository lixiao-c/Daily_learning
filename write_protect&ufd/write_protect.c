#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/mman.h>
#include<sys/syscall.h>
#include<signal.h>

#include<linux/userfaultfd.h>
#define LEN 1024*16

// write protection handler
void handler(int sig,siginfo_t *si,void *unused)
{
    unsigned long addr=si->si_addr;
    printf("addr %x being written\n",addr);
    return;
}
int main()
{
    int i;
    int fd;
    fd=open("write_file",O_RDWR|O_CREAT);
    if(fd<0)
        printf("error in open fd\n");
    char buf[LEN];
    for(i=0;i<LEN;i++)
    {
        buf[i]=i%1024;
    }
    int err=pwrite(fd,buf,LEN,0);
        printf("write %d\n",err);
    fsync(fd);
    
    // set write protection
    char* addr=mmap(NULL,LEN,PROT_READ,MAP_SHARED,fd,0);
    
    //registe write pro handler 
    struct sigaction sa;
    sa.sa_flags=SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction=handler;
    if(sigaction(SIGSEGV,&sa,NULL)==-1)
        printf("error\n");
    // test write handler
    *(addr)=1;

}
