## mmap使用方法
### 一、概述
mmap函数会将文件或者设备映射到内存的地址空间上，编程时，可使用**open+mmap+memset+msync+munmap**代替**open+write/read+close**进行文件访问。除此之外，mmap在PM编程中也是重要的访问手段，还可以用来实现共享内存、写保护的处理、userfault等。其函数原型如下：
```
 #include <sys/mman.h>
  void *mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset);
```
- addr是程序员指定映射到内存地址，但mmap不一定会精确的映射到该地址，只是会尽量映射地址附近的位置。addr一般为NULL，此时系统会选择一块地址进行映射。
- length是映射的长度
- prot是为映射内存设置的权限
- flags可以决定该映射是否被多个进程共享，是否匿名映射等等
- fd是映射的后端文件的文件描述符
- offset是映射文件的开始偏移量
- 返回值返回映射的开始地址


下面具体解释prot和flags的用法
- PROT

prot可以为映射内存设置权限，主要有一下四种权限：

**PROT_READ**  内存页是可读的

**PROT_WRITE** 内存页是可写的

**PROT_EXEC**  内存是可执行的

**PROT_NONE**  内存是不可访问的

这几种设置可以进行OR，例如设置为PROT_READ|PROT_WRITE。

- FLAGS

FLAGS列举一下几种：

**MAP_SHARED** 与其他进程共享这片内存映射(映射统一文件区域),对该区域的更新其他进程也可见

**MAP_PRIVATE** 对该区域的更新，其他进程不可见。

**MAP_ANONYMOUS** 映射匿名页，不需要后端的文件(fd=-1),映射的内存初始化为0。

**MAP_POPULATE** 在调用mmap映射时，就将真实的数据映射到内存上（read ahead）,访问时不会产生缺页。

下文将介绍几个mmap的使用情形。

---
[1]http://man7.org/linux/man-pages/man2/mmap.2.html
[2]http://blog.decaywood.me/2017/04/10/Linux-mmap/

### 二、mmap性能

在默认使用mmap时，mmap并不会真的将文件的内容拷贝到内存中，而是等到读取相应内存时，触发缺页中断，才会读取文件。当使用MAP_POPULATE的标志时，mmap则会一开始就将文件内容拷贝到内存中，在读取时就不会有缺页的现象了。

对此，有研究表明使用mmap读取文件的速度并不一定比传统的read快，甚至性能还略有下降。在使用Ext4-DAX的系统中，以4K大小顺序读4G文件，最后花费的时间如下图：
![mmap read对比图](https://github.com/lixiao-c/Daily_learning/blob/master/mmap%26read.PNG)

造成这种情况的原因主要是频繁的用户态与内核态的切换。每次读4k，在使用默认的mmap时，几乎每一次都会缺页，造成用户态和内核态的频繁切换，如果是更细粒度的读，估计这种情况会大大缓解。

在参考文献中的论文中，提出来mmap-ahead的方式，仿照read-ahead进行预取，节省状态切换的时间，不失为一种好的解决办法。

---
[1]Jungsik Choi, Jiwon Kim, Hwansoo Han,"Efficient Memory Mapped File I/O for In-Memory File Systems"

### 三、PM编程

PM编程一般可使用mmap，先对PM上的数据进行内存映射，然后根据映射的地址进行数据的操作。但是由于PM设备与mem都使用DIMM接口和内存总线，mmap并不真正将PM内容copy到DRAM上，只是建立映射关系(利用页管理方式)。一般的编程方式如下：
```
1. addr=mmap(NULL,LEN,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
2. memcpy(addr,...);
   memset(addr,...);
   *(addr+i)=...;
3. _mm_clflush(addr);
   _mfence();
```
在对数据操作完成后，需要进行持久化，可以使用原有的msync()接口，但是该接口不能充分发挥PM的性能优势，所以一般使用clflush命令，该命令会将cache中的一条cache line写回PM中，mfence()是内存屏障，由于cache的刷回操作一般是乱序的，在进行其他操作之前，使用mfence保证在其之前的store都已完成。

---
[1]Andy Rudoff. "Persistent Memory Programming".

### 四、共享内存
使用MAP_SHARED可以在多个进程之间共享映射的内存区域，并可以借助共享内存实现进程间通信。使用mmap映射同一文件后端，可以实现进程间的内存共享。
```
receive.c
	addr=mmap(NULL,LEN,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    while(1){
        if(*(addr)=='c'){
            printf("get message from another process\n");
            return;
        }
        printf("addr value %x \n",*(addr));
    }

send.c
    addr=mmap(NULL,LEN,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
   *(addr)='c';
```
也可以对匿名映射使用共享内存，但应该使用fork生成子进程，在父子进程之间进行共享内存通信。

### 五、段错误和总线错误
(参考文章：http://blog.jcix.top/2018-10-26/mmap_tests/ )

mmap可以在一定程度上取代write/read，成为操作文件的接口，但相比之下，mmap无法像write一样改变文件大小。同时由于mmap采用了地址映射，经常会出现段错误和总线错误。在本小节中探究造成两种错误的原因。

**段错误**

“文件范围内, mmap范围外”，会产生SIGSEGV段错误。当我们映射的区域小于文件大小时，访问mmap范围外会触发段错误。这种情况很容易理解，因为我们访问了非法的内存区域。

此外，当mmap时，PORT只设置为PROT_READ，会对mmap区域加上写保护，当写mmap映射区域时，也会触发段错误。如果一开始没加写保护，还可以使用mprotect()修改内存保护位。

**总线错误**

”文件范围外,mmap范围内” 会产生SIGBUS总线错误。如前文所说，mmap不能修改文件大小，因此，当mmap映射区域大于文件范围，向文件范围外进行写操作时，会首先触发缺页，在处理缺页时，检查到写的区域超过了文件大小，抛出总线错误。

对于稀疏文件（具体可查看参考文章内容），对稀疏文件内部的文件空洞进行访问不会造成总线错误。

### 六、写保护处理
当使用mmap函数时，我们会设置‘port’、‘flags’等参数，其中port参数会设置mmap内存的保护信息，一般的port会设置为PROT_READ|PROT_WRITE，表示读写都可以进行，当只设置PROT_READ时，便为内存构建了写保护。此时对映射的内存区域进行写操作，便会触发经典错误：段错误（segment fault）。但是我们可以根据信号机制，对触发write protection区域进行处理。

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
[2]mprotect. http://man7.org/linux/man-pages/man2/mprotect.2.html
