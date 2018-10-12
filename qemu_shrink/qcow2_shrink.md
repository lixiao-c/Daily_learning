# qcow2等虚拟机镜像的空间回收机制
## 1.问题背景
在虚拟化场景下，瘦分配(Thin-provisioning)磁盘应用场景非常广泛。目前主流的虚拟磁盘镜像格式，如Dynamic VHD、Sparse Raw、Qcow2、VMDK等均只具有随着虚拟机读写而动态增长的能力，一般来说是按需每次分配一个固定大小的块，如qcow2是以cluster为单位的。但是，当虚拟机长时间使用后，其空间会变的越来越大，即使在guest中进行了文件删除，host中的镜像却不会变小。
### 1.1 qcow2的测试
以qcow2为例，可以进行测试。对同一个镜像进行测试，在guest中创建一个2G大小的文件，然后删除该文件，并在host中检验相应的qcow2镜像大小。
```
如以下两图所示，图1比图2在guest中多使用了2G的磁盘空间
```
![figure 1](https://i.imgur.com/RaHzJTP.png)
>Figure1

![](https://i.imgur.com/7AiBq9u.png)
>Figure2

但是在host中，其镜像大小都是6.4G。并且对qcow2而言，其元数据只要被写过一次，之后使用时都只会被重用，不会重新写。换言之，对元数据的在PM上的写优化没起什么作用。
### 1.2原因
**猜想1**

文件系统在删除时只会删除相应的节点，不会真正的删除数据，所以guest的删除操作不会传导到下层的镜像。

**猜想2**

即使qemu做了一些工作，可以使得rm操作传导到镜像层，但出于I/O性能的考虑，也会选择进行重用。
## 2.镜像空间回收
当Guest OS删除了文件，已经分配的空间在虚拟磁盘上可否进行空间回收呢？这样做可以腾出更多的存储空间(离线回收)。
如果对qcow2空间进行回收，可以达到这样两种效果：
(1) 在Host上qcow2占用空间减少（离线空间回收）；
(2) 在Host上qcow2占用空间未减少，但后续虚机写入的数据可以复用回收的空间，而不需要重新分配新的块（在线空间回收)
### 2.1离线空间回收
linux下的步骤：
>启动虚拟机，创建一个文件
touch tmpfile
dd if=/dev/zero of=/tempfile
将0填充到创建的tmpfile中，直到0占满整个空间，需要停止所有进程
rm -f /tempfile
停止虚拟机
mv image.qcow2 image.qcow2_backup
qemu-img convert -O qcow2 image.qcow2_backup image.qcow2
qemu-img convert -O qcow2 -c image.qcow2_backup image.qcow2

原理很简单：手动将虚机磁盘的资源回收(linux可用dd命令，windows可用sdelete工具)，然后关机，将qcow2文件利用qemu-img的convert命令转化为另一块新盘并替换掉。

### 2.2在线空间回收
>(1) 原理：利用的TRIM功能，其原理下个小节细说
 (2) 优点：不需要暂停虚机，不影响业务
           理论上读写性能会提高
 (3) 缺点：qcow2文件在Host上实际占用空间不会减少，但后续虚机写数据时会复用之前的空间
           数据恢复，由于对删除文件的空间进行了复用，故如在虚机中对虚拟盘做数据恢复，难度会增大

>在线空间回收实现：
 (1) 在nova-compute.conf中的libvirt段中增加以下属性：
     hw_disk_discard=unmap，使其虚机配置文件的disk具有discard='unmap'属性
 (2) 对镜像增加这如下两个属性：
     hw_disk_bus = scsi
     hw_scsi_model = virtio-scsi
     使其虚机配置文件的disk的bus为scsi，并且scsi的model为virtio-scsi
 (3) Linux版本镜像mount ext4时增加 -o discard选项，如果是ext3,ext2文件系统，则加fstrim -av定时器即可；windows版本镜像确保fsutil behavior query DisableDeleteNotify 查询结果为0

### 2.3相关文献
[1]中给出了离线和在线回收的总结。[2]是一个wiki，给出了具体的操作方法和一些说明，[3]是[2]的简化版翻译。

---
### 参考文献
[1]Qcow2文件的空间回收研究总结, http://p.sihuatech.com/w/xview/qcow2%E6%96%87%E4%BB%B6%E7%9A%84%E7%A9%BA%E9%97%B4%E5%9B%9E%E6%94%B6/

[2]Shrink Qcow2 Disk Files, https://pve.proxmox.com/wiki/Shrink_Qcow2_Disk_Files

[3] https://www.jianshu.com/p/f623ee6ac41d

