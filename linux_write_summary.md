## linux write总结
linux提供了各种不同的io接口，本文主要介绍linux提供的各种write接口。主要包括：异步aio，write，writev，pwrite，pwritev。然后与之相关的sync机制，包括：sync，fsync，fdatasync。
### 1.write 
**1.1 write**

write接口是linux提供的最原始的io写接口，其函数原型如下：
```
#include <sys/unistd.h>
ssize_t write(int fd, const void *buf, size_t count);
```
将buf中指定count的数据写入到fd确定的文件中。

但由于内存和二级存储之间高速缓存的存在，write调用一般不会真正将数据写入文件，而是写入高速缓存后就返回，所以出现了sync系调用来保证数据真正被写回磁盘。(但linux在后期还提供了绕过高速缓存的写方式，如fio中可将direct设置为1，则io会绕过高速缓存)

**1.2 writev**

writev,从字面上来理解，就是write vector。函数原型如下：
```
#include <sys/uio.h>
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
```
将iovcnt个iovec写入到fd所确定的文件中去。下面解释一下什么时iovec。 iovec即io vector：
```
struct iovec {
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
};
```
这里不得不提到writev的设计初衷，write只能写一个buffer中的数据，当要写多个不连续的数据块时，多次调用write会造成大量的数据浪费。iovec可以记录多个buffer的地址，以及要写入的大小。通过调用一次writev，节省了大量的系统调用开销。

**1.3 pwrite**

函数原型如下：
```
#include<sys/unistd.h>
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
```
pwrite同样是将buf中的count大小的数据写入fd所确定的文件中去，但与write不同的是其可以写入文件的特定位置，即offset所确定的位置。

**1.4 pwritev**

pwritev从字面上也可以看出，是pwrite和writev的结合。
```
#include<sys/uio.h>
ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt,
                off_t offset);
```

**1.5 异步aio**

异步aio是当下linux使用最广的io方式。上文中介绍的都属于同步io，io API调用后，都会返回相应的结果，而异步io则不能立即得到相应的结果。aio是异步非阻塞io，即io调用后会立即返回，说明io已经发起了，但io的结果并没有得到，在下层处理io的时间内，可以执行其他程序，当结果得到后，会通知程序，io完成。如下图（read）所示：

![](https://i.imgur.com/mbgAOmv.gif)

在异步非阻塞 I/O 中，我们可以同时发起多个传输操作。这需要每个传输操作都有惟一的上下文，这样我们才能在它们完成时区分到底是哪个传输操作完成了。在 AIO 中，这是一个 aiocb（AIO I/O Control Block）结构。
```
struct aiocb {
 
  int aio_fildes;               // File Descriptor
  int aio_lio_opcode;           // Valid only for lio_listio (r/w/nop)
  volatile void *aio_buf;       // Data Buffer
  size_t aio_nbytes;            // Number of Bytes in Data Buffer
  struct sigevent aio_sigevent; // Notification Structure
 
  /* Internal fields */
  ...
 
};
```
sigevent 结构告诉 AIO 在 I/O 操作完成时应该执行什么操作。

aio的API如下：
- aio_read	    请求异步读操作
- aio_error	    检查异步请求的状态
- aio_return	获得完成的异步请求的返回状态
- aio_write	    请求异步写操作
- aio_suspend	挂起调用进程，直到一个或多个异步请求已经完成（或失败）
- aio_cancel	取消异步 I/O 请求
- lio_listio	发起一系列 I/O 操作
---
### 2.sync操作

一般情况下，对硬盘（或者其他持久存储设备）文件的write操作，更新的只是内存中的页缓存（page cache），而脏页面不会立即更新到硬盘中，而是由操作系统统一调度，如由专门的flusher内核线程在满足一定条件时（如一定时间间隔、内存中的脏页达到一定比例）内将脏页面同步到硬盘上（放入设备的IO请求队列）。

因为write调用不会等到硬盘IO完成之后才返回，因此如果OS在write调用之后、硬盘同步之前崩溃，则数据可能丢失。通常需要OS提供的同步IO（synchronized-IO）原语来保证持久性和一致性。

**2.1 sync**
```
#include <unistd.h>
void sync(void);
int syncfs(int fd);
```
sync函数只是将所有修改过的块缓冲区排入写队列，然后就返回，它并不等待实际写磁盘操作结束。

**2.2 fsync**
```
#include <unistd.h>
int fsync(int fd);
```
fsync会将修改过的数据同步到fd所确定的文件中。fsync通常包括两次flush，一次flush数据，另一次flush元数据

**2.3 fdatasync**
```
#include <unistd.h>
int fsync(int fd);
int fdatasync(int fd);
```
除了同步文件的修改内容（脏页），fsync还会同步文件的描述信息（metadata，包括size、访问时间st_atime & st_mtime等等），因为文件的数据和metadata通常存在硬盘的不同地方，因此fsync至少需要两次IO写操作。

fdatasync的功能与fsync类似，但是仅仅在必要的情况下才会同步metadata，因此可以减少一次IO写操作。即：
>fdatasync does not flush modified metadata unless that metadata is needed in order to allow a subsequent data retrieval to be corretly handled

**2.4 msync**
```
#include <sys/mman.h>
int msync(void *addr, size_t length, int flags);
```
msync和fsync相似，但是msync针对的是通过mmap映射的文件。

---
### 附录A fio中各引擎对应的io方式

- libaio Linux 原生的异步 I/O，这也是通常我们这边用的最多的测试盘吞吐和延迟的方法
- sync 也就是最通常的 read / write 操作
- vsync 使用 readv / writev，主要是会将相邻的 I/O 进行合并
- psync 对应的 pread / pwrite
- pvsync / pvsync2 对应的 preadv / pwritev，以及 preadv2 / pwritev2

---
### 参考文献
[1]https://linux.die.net/man/2/pwritev

[2]https://linux.die.net/man/2/pwrite

[3]https://linux.die.net/man/2/writev

[4]https://linux.die.net/man/2/write

[5]https://linux.die.net/man/1/fio

[6]使用 fio 进行 IO 性能测试.https://www.jianshu.com/p/9d823b353f22

[7]慢慢聊linux Aio. https://blog.csdn.net/zhxue123/article/details/21087165

[8]使用异步 I/O 大大提高应用程序的性能. https://www.ibm.com/developerworks/cn/linux/l-async/index.html

[9]https://linux.die.net/man/7/aio

[10]https://linux.die.net/man/2/fdatasync

[11]https://linux.die.net/man/2/fsync

[12]https://linux.die.net/man/2/sync

[13]https://linux.die.net/man/2/msync