# PM一致性和原子性问题（一）
---
## 一、PM的原子更新的单位
多篇论文中都提到，	PM的原子更新单位是8byte（intel环境下），如下所示：
```
***1***
On Intel, only an eight-byte store, aligned on an eight-byte boundary,
is guaranteed to be failure atomic. 

ANDY RUDOFF,"Persistent Memory Programming".

***2***
the failure atomicity unit for persistent
memory is generally expected to be 8 bytes or no larger
than a cache line
……
However,with failure-atomic write granularity of 8 bytes in PM …

HWANG, D., KIM, W.-H., WON, Y., AND NAM, B. Endurable
transient inconsistency in byte-addressable persistent b+-tree. In
Proceedings of the 16th USENIX Conference on File and Storage
Technologies (FAST) (2018)

***3***
the atomic write unit for modern processors is
generally no larger than the memory bus width (e.g.,
8 bytes for 64-bit processors)

Pengfei Zuo, Yu Hua, Jie Wu. Write-Optimized and High-Performance
Hashing Index Scheme for Persistent Memory.OSDI(2018)
```
而且不约而同的提到要使用clflush将cache line刷新到PM、cfence保证flush(store)操作不被打乱顺序 这两点，保证数据可以在PM上持久化。

根据以上两点，我们可以发现一个问题，cache line的大小通常为64byte，但原子性更新的最小单位为8byte。我们调用clflush一次会将64byte的数据写会PM，这个原子更新的最小单位之间有什么联系呢？为什么cache line不是原子更新单位呢。

现代的处理器，memory数据总线宽度为64bit，即8byte，也就是说，每一次访问memory只能读或写8byte。那么cache是怎么刷新cache line的呢。参考stack overflow和wiki百科上的答案，
```
Modern PC memory modules transfer 64 bits (8 bytes) at a time, 
in a burst of eight transfers, 
so one command triggers a read or write of a full cache line from memory.
```
即对于cache line的刷新操作而言，cache(SDRAM)会进行一次burst操作，burst会包含8次transfer，每次transfer会传输总线宽度的数据回内存。在这种机制下，就可以理解，如果在数据传输时出现power loss，只能保证8byte的原子更新。
### （一）clflush指令的探究
clflush，intel中的实现叫做_mm_clflush()，这不是一个函数，而是一个指令。它的作用：
```
在处理器缓存层次结构（数据与指令）的所有级别中，使包含源操作数指定的线性地址的缓存线失效。
失效会在整个缓存一致性域中传播。
如果缓存层次结构中任何级别的缓存线与内存不一致（污损），则在使之失效之前将它写入内存。
```
我们都知道，一条指令的执行过程，一般是不可被中断的。那么可以推测出，clflush这条指令在软件层面上来看，是一个原子性的操作，但实际上，clflush会使得cache进行burst操作，burst操作可以被power fail打断。
### （二）burst的探究
burst是由SDRAM的controller控制的，具体可见wiki百科[1]。burst会进行不止一次的transfer操作，那么这些transfer操作之间的顺序是如何确定的呢？
```
 A cache line fetch is typically triggered by a read from a particular address, 
and SDRAM allows the "critical word" of the cache line to be transferred first.
```
上段描述意味着burst可以允许“关键词优先”的传输。这里虽然说的是读，但对于写也是一样的。
```
for example, a four-word burst access to any column address from four to seven will return words four to seven.
 The ordering, however, depends on the requested address, and the configured burst type option: sequential or interleaved. 

……

For the sequential burst mode, later words are accessed in increasing address order, wrapping back to the start of
the block when the end is reached. So, for example, for a burst length of four, and a requested column address of
five, the words would be accessed in the order 5-6-7-4. If the burst length were eight, the access order would be
5-6-7-0-1-2-3-4. This is done by adding a counter to the column address, and ignoring carries past the burst
length. The interleaved burst mode computes the address using an exclusive or operation between the counter and the
address. Using the same starting address of five, a four-word burst would return words in the order 5-4-7-6. An
eight-word burst would be 5-4-7-6-1-0-3-2.[2] Although more confusing to humans, this can be easier to implement in
hardware, and is preferred by Intel for its microprocessors.
```
上段话说明了burst的两种传输顺序策略，一种是顺序传输，另一种是交错传输。但无论哪一种，都是讲请求的address优先传输。
### （三）clflush的再思考
虽然我们不知道从clflush到burst之间是怎样的流程(基本上是硬件过程)，但我们可以大胆猜测，clflush中要刷新的地址被传递到下层的burst时也应该是相应的请求的address。举例而言，clflush(5)--- > burst(5)，这就保证了，我们期望刷回PM的原子性单位会被首先刷回，不会出现clflush调用后因power fail造成的不一致现象。

---
## 二、PM的崩溃一致性
由于原子更新的单位是8byte所以大于8byte的数据都要进行log或COW来保证崩溃一致性。在[2]的论文中，也提到了这一点。
```
the atomic write unit for modern processors is
generally no larger than the memory bus width (e.g.,
8 bytes for 64-bit processors) [17, 20, 24, 60]. If the
written data is larger than an atomic write unit, we need
to employ expensive logging or copy-on-write (CoW)
mechanisms to guarantee consistency
```

---
### 参考文献
[1]SDRAM, https://en.wikipedia.org/wiki/Synchronous_dynamic_random-access_memory#Commands

[2]Pengfei Zuo, Yu Hua, Jie Wu. Write-Optimized and High-Performance Hashing Index Scheme for Persistent Memory. In Proceedings of 13th USENIX Symposium on Operating Systems Design and Implementation (OSDI) (2018)

[3]https://stackoverflow.com/questions/3928995/how-do-cache-lines-work

[4]https://stackoverflow.com/questions/39182060/why-isnt-there-a-data-bus-which-is-as-wide-as-the-cache-line-size

[5]LEE, S. K., LIM, K. H., SONG, H., NAM, B., AND NOH, S. H. WORT: Write Optimal Radix Tree for Persistent Memory Storage Systems. In Proceeding of the USENIX Conference on File and Storage Technologies (FAST) (2017).

[6]ANDY RUDOFF. Persistent Memory Programming.

