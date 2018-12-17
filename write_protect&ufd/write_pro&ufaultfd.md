## write protection 处理和 userfaultfd
### 一、write protection 处理
```
#include <sys/mman.h>
void *mmap(void *addr, size_t lengthint " prot ", int " flags ,
           int fd, off_t offset);int munmap(void *addr, size_t length);
```
当使用mmap函数时，我们会设置‘port’、‘flags’等参数，其中port参数会设置mmap内存的保护信息，主要有一下几种：
```
PROT_EXEC
Pages may be executed.
PROT_READ
Pages may be read.
PROT_WRITE
Pages may be written.
PROT_NONE
Pages may not be accessed.
```
一般的port会设置为PROT_READ|PROT_WRITE，表示读写都可以进行，当只设置PROT_READ时，便为内存构建了写保护。此时对映射的内存区域进行写操作，便会触发经典错误：段错误（segment fault）。但是我们可以根据信号机制，对触发write protection区域进行处理。

信号是进程间通信的一种方式，又称为软中断，因此和中断类似，信号也有自己的处理函数。处理方法可以分为三类：第一种是类似中断的处理程序，对于需要处理的信号，进程可以指定处理函数，由该函数来处理。第二种方法是，忽略某个信号，对该信号不做任何处理，就象未发生过一样。第三种方法是，对该信号的处理保留系统的默认值，这种缺省操作，对大部分的信号的缺省操作是使得进程终止。

当访问写保护内存时，会发出SIGSEGV信号，即“无效的内存引用”。该信号没有默认的处理函数，所以出发后会抛出段错误，进程终止，我们可以为信号设置相应的处理函数。

sigaction函数可以改变信号的处理，即“ examine and change a signal action ”。函数原型如下：
```
#include <signal.h>
int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact);
```
因此，我们需要构造一个sigaction结构体，该结构体原型如下：
```
   struct sigaction {
               void     (*sa_handler)(int);
               void     (*sa_sigaction)(int, siginfo_t *, void *);
               sigset_t   sa_mask;
               int        sa_flags;
               void     (*sa_restorer)(void);
           };
```
sa_handler和sa_sigaction都是处理函数，具体使用可以参考man sigaction，处理写保护时，需要设置sa_sigaction。利用以下代码进行信号处理注册。
```
//registe write pro handler 
struct sigaction sa;
sa.sa_flags=SA_SIGINFO;
sigemptyset(&sa.sa_mask);
sa.sa_sigaction=handler;
if(sigaction(SIGSEGV,&sa,NULL)==-1)
    printf("error\n");
```
这样，当我们访问写保护地址时，便会转到handler函数处理。在代码中(write_protect&ufd/write_protect.c)，简单实现了一个handler函数，打印访问的地址。

---
[1]sigaction. http://man7.org/linux/man-pages/man2/sigaction.2.html

### 二、userfaultfd
userfaultfd 机制让在用户控制缺页处理提供可能，进程可以在用户空间为自己的程序定义page fault handler。

---
参考可见：

[0]http://blog.jcix.top/2018-10-01/userfaultfd_intro/

[1] USERFAULTFD(2), http://man7.org/linux/man-pages/man2/userfaultfd.2.html

[2] IOCTL_USERFAULTFD(2), http://man7.org/linux/man-pages/man2/ioctl_userfaultfd.2.html

[3] The next steps for userfaultfd(), https://lwn.net/Articles/718198/

[4] [PATCH 0/3] userfaultfd: non-cooperative: syncronous events, https://lkml.org/lkml/2018/2/27/78

[5] [RFC PATCH v2] userfaultfd: Add feature to request for a signal delivery, https://marc.info/?l=linux-mm&m=149857975906880&w=2

[6] Live migration – Wikipedia, https://en.wikipedia.org/wiki/Live_migration

[7] Userfaultfd: Post-copy VM migration and beyond, https://blog.linuxplumbersconf.org/2017/ocw//system/presentations/4699/original/userfaultfd_%20post-copy%20VM%20migration%20and%20beyond.pdf