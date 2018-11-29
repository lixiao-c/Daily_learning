## qcow2写放大问题原理分析
### ——Mitigating Sync Amplification for Copy-on-write Virtual Disk，FAST16
## 1 论文表述
本篇论文在qcow2格式镜像的io上观察到了fdatasync的写放大问题，即guest用户态的一次fdatasync操作会引起host上的多次fdatasync。写放大的原因主要有两点。一是ext4之类的日志文件系统的日志记录操作，如图1。而是由于qcow2的格式问题，带来维护其一致性的开销，如图2所示。下文将主要从源代码的角度来具体分析写放大问题。

图1
![图1](https://i.imgur.com/E49Felu.png)

图2
![](https://i.imgur.com/S26qPN1.png)
## 2 日志文件系统
以ext4文件系统为例，其内置了一个日志系统——jdb2。该日志系统可以在系统crash时保护主要的数据。它会在磁盘开辟一块区域来存放日志(默认为128M)。可以看出，这块区域并不大，不可能将所有数据都进行日志记录。对于文件系统而言，最重要的数据是metadata，所以其只会对元数据进行日志记录。所以Jm(jounal of metadata)要被立即flush到磁盘上。当日志记录完成，要进行commit操作，又需要第二次的flush操作[2]。
## 3 qemu、qcow2的io操作
guest中的io是如何传递到qemu中的呢？这需要virtio的支持。virtio的前端获得guest的io请求，然后将请求发送到后端进行处理。后端的处理函数在block\blk-backend.c中。其中有两个重要的函数：
```
static void blk_aio_flush_entry(void *opaque)
static void blk_aio_write_entry(void *opaque)
```
（注：这里是aio的原因猜测是由于linux使用原生的aio。也可能会调用blk_flush_entry、blk_write_entry等接口，但都会调用相同的io函数）
可以猜测其实guest io中write、flush的后端处理。最终flush会调用block\io.c\bdrv_co_flush,write会调用block\io.c\bdrv_co_pwritev,进而调用qcow2_co_pwritev。
### 3.1 虚拟机缓存模式
在分析该函数之前，首先要弄清楚虚拟机的磁盘缓存模式。具体可参考[3][4]。目前，qemu的默认缓存模式是cache=writeback[1][4]。这意味着write操作将数据写到host的page cache就算成功，同时支持guest发出的flush操作。那么这种操作是如何实现的呢？在block.c中：
```
int bdrv_parse_cache_mode(const char *mode, int *flags, bool *writethrough)
{
    *flags &= ~BDRV_O_CACHE_MASK;

    if (!strcmp(mode, "off") || !strcmp(mode, "none")) {
        *writethrough = false;
        *flags |= BDRV_O_NOCACHE;
    } else if (!strcmp(mode, "directsync")) {
        *writethrough = true;
        *flags |= BDRV_O_NOCACHE;
    } else if (!strcmp(mode, "writeback")) {
        *writethrough = false;
    } else if (!strcmp(mode, "unsafe")) {
        *writethrough = false;
        *flags |= BDRV_O_NO_FLUSH;
    } else if (!strcmp(mode, "writethrough")) {
        *writethrough = true;
    } else {
        return -1;
    }
    return 0;
}
```
如果是cache=writeback，不设置writethrough标志；如果是cache=unsafe，会设置标签BDRV_O_NO_FLUSH。之后到blockdev.c中，有：
```
if (!qemu_opt_get(all_opts, BDRV_OPT_CACHE_WB))
qemu_opt_set_bool(all_opts, BDRV_OPT_CACHE_WB, !writethrough, &error_abort);
……
writethrough = !qemu_opt_get_bool(opts, BDRV_OPT_CACHE_WB, true);
……
blk_set_enable_write_cache(blk, !writethrough);
```
这三段代码对blk->enable_write_cache进行了传递，最终将blk->enable_write_cache设置为!writethrough。如果没有设置enable_write_cache，会多加一个标签BDRV_REQ_FUA。
```
if (!blk->enable_write_cache)
    flags |= BDRV_REQ_FUA;
```
如果该标签设置了，在bdrv_driver_pwriteve的最后就会调用bdrv_co_flush,实现writethrough的效果。
```
emulate_flags:
    if (ret == 0 && (flags & BDRV_REQ_FUA)) {
        printf("io writethrough -> bdrv_co_flush\n");
        ret = bdrv_co_flush(bs);
	}
```
### 3.2 bdrv_co_flush
我们以qcow2镜像，cache=writeback来讲解bdrv_co_flush函数。首先会进入bdrv_co_flush中bs->drv->bdrv_co_flush_to_os,该函数会负责flush虚拟磁盘模拟的硬件缓存[5]。在这里主要是l2 cache和ref cache。功能如其名，此函数只是调用bdrv_pwritev将l2 cache、ref cache写到host的page cache。

由于qcow2镜像没有bdrv_co_flush_to_disk、bdrv_aio_flush函数，所以程序会运行到
```
flush_parent:
    ret = bs->file ? bdrv_co_flush(bs->file->bs) : 0;
```
再次调用bdrv_co_flush。这一次的flush是对文件的flush了。我们将其称为brdv_co_flush2。brdv_co_flush2会调用bdrv_aio_flush，即block\file-posix.c中的raw_aio_flush。然后通过paio_submit提交QEMU_AIO_FLUSH命令。在aio_worker中调用handle_aiocb_flush，最终调用**qemu_fdatasync**。

函数栈如下：
```
   -  blk_aio_flush_entry                                                                                                                                                                                                              
      -  blk_co_flush                                                                                                                                                                                                                  
         -  bdrv_co_flush                                                                                                                                                                                                              
            -  qcow2_co_flush_to_os                                                                                                                                                                                                    
               -  qcow2_write_caches                                                                                                                                                                                                   
                  -  qcow2_cache_write                                                                                                                                                                                                 
                        qcow2_cache_entry_flush                                                                                                                                                                                        
              bdrv_co_flush 
				- 	raw_aio_flush
					- 	paio_submit ---> aio_worker
											- handle_aiocb_flush
												- qemu_fdatasync	
```
### 3.3  qcow2_co_pwritev
我们还是只关心该函数中进行flush的代码段。最容易找到的flush操作出现在refcount block的分配、l2的分配、l1的增长、写操作。 如l1的增长调用bdrv_pwrite_sync-->bdrv_flush。l2、ref的分配调用qcow2_cache_flush(两次)--> bdrv_flush。

与此同时，qcow2_co_pwritev还为cache设置了复杂的依赖关系。在link_l2-->perform_cow2-->qcow2_cache_depends_on_flush(s->l2_table_cache),将l2_tabel_cache的depends_on_flush设置为true。让我们回到qcow2_co_flush_to_os，会先调用cache_write写会l2 cache，然后写ref cache。在写cache的函数qcow2_cache_entry_flush中，会先判断cache的depends_on_flush，如有，则进行bdrv_flush。
```
block\qcow2-cache.c

    if (c->depends) {
        ret = qcow2_cache_flush_dependency(bs, c);
    } else if (c->depends_on_flush) {
        ret = bdrv_flush(bs->file->bs);
        if (ret >= 0) {
            c->depends_on_flush = false;
        }
    }
```

## 4 分析
![](https://i.imgur.com/S26qPN1.png)
①  qcow2_co_pwritev-->update_refcount-->alloc_refcount_block-->qcow2_cache_flush--> bdrv_flush-->fdatasync

②  qcow2_co_pwritev-->get_cluster_table-->l2_allocate-->qcow2_cache_flush--> bdrv_flush-->fdatasync

③  qcow2_co_pwritev-->qcow2_grow_l1_table-->bdrv_pwrite_sync-->bdrv_flush-->fdatasync

④  qcow2_co_pwritev-->link_l2-->perform_cow2-->qcow2_cache_depends_on_flush-->depends_on_flush=true
   bdrv_co_flush-->bdrv_co_flush_to_os-->qcow2_cache_write-->if depends_on_flush=true  bdrv_flush-->fdatasync

⑤  bdrv_co_flush-->bdrv_co_flush-->raw_aio_flush-->fdatasync

(序号指flush的序号)

## 5 实验

我们在guest上进行验证。“a.out”会进行写操作，然后调用sync，打印函数调用的相关信息。
```
[clx@localhost ~]$ ./a.out
start test
start pwrite
close fd
[clx@localhost ~]$ sync
came in qcow2_co_pwritev    //write1  
leave qcow2_co_pwritev
came in blk_aio_flush_entry  //第一次flush flush应该紧跟在write之后
came in bdrv_co_flush
came in qcow2_flush_to_os    //flush to os
came in qcow2_write_caches   // write l2
came in qcow2_cache_write    //此处数据量太小，如果有ref的更改，应该还有flush
leave qcow2_cache_write
came in qcow2_cache_write    //write ref
leave qcow2_cache_write
leave qcow2_write_caches
leave qcow2_flush_to_os
came in bdrv_co_flush        //第二次 bdrv_co_flush
came in raw_aio_flush
came in handle_aiocd_flush
came in qemu fdatasync      // fdatasync
leave bdrv_co_flush
leave bdrv_co_flush

came in qcow2_co_pwritev  //第二次flush
leave qcow2_co_pwritev
came in blk_aio_flush_entry
came in bdrv_co_flush
came in qcow2_flush_to_os
came in qcow2_write_caches
came in qcow2_cache_write
leave qcow2_cache_write
came in qcow2_cache_write
leave qcow2_cache_write
leave qcow2_write_caches
leave qcow2_flush_to_os
came in bdrv_co_flush       
came in raw_aio_flush
came in handle_aiocd_flush
came in qemu fdatasync
leave bdrv_co_flush
leave bdrv_co_flush
```

---
### 参考文献
[1]Qingshu Chen, Liang Liang, Yubin Xia, Haibo Chen, Hyunsoo Kim. Mitigating Sync Amplification for Copy-on-write Virtual Disk. FAST16

[2]https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout#Journal_.28jbd2.29

[3]http://www.cnblogs.com/sammyliu/p/5066895.html

[4]https://www.suse.com/documentation/sles11/book_kvm/data/sect1_1_chapter_book_kvm.html

[5]http://oenhan.com/qemu-block-cache