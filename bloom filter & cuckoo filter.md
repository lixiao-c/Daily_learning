## Bloom filter
Bloom filter是一种有趣的数据结构，可以用于检索一个元素是否在一个集合中。
Bloom Filter有可能会出现错误判断，但不会漏掉判断。也就是Bloom Filter判断元素不再集合，那肯定不在。如果判断元素存在集合中，有一定的概率判断错误。因此，Bloom Filter不适合那些“零错误”的应用场合。而在能容忍低错误率的应用场合下，Bloom Filter比其他常见的算法（如hash，折半查找）极大节省了空间。 
### 基本思想
Bloom filter使用了hash的思想，但与一般的hash函数不同的是，其使用多个hash用来减少冲突。
当判断一个元素是否已经出现时，能够想到比较快速和简介的做法就是hash，但单个hash函数的冲突过多，进而造成大概率的判断错误，于是Bloom filter采用多个hash函数。
### 算法
**step 0**
创建一个m位BitSet，先将所有位初始化为0，然后选择k个不同的哈希函数。第i个哈希函数对字符串str哈希的结果记为h（i，str），且h（i，
str）的范围是0到m-1 。

**记录字符串的出现**
对于字符串str，分别计算h（1，str），h（2，str）…… h（k，str）。然后将BitSet的第h（1，str）、h（2，str）…… h（k，str）位设为1。
![](https://i.imgur.com/W8YiQcY.jpg)

**检查字符串是否出现**
对于需要检查的字符串，仍然分别计算各个hash函数的值，并检查对应的位是否为1，如果出现任意一个hash值为1但Bitset相应位不为1，则说明该字符串未出现过。
但注意，若一个字符串对应的Bit全为1，实际上是**不能100%的肯定该字符串被Bloom Filter记录过**。（因为有可能该字符串的所有位都刚好是被其他字符串所对应）这种将该字符串划分错的情况，称为false positive 。

**字符串的删除**
字符串加入了就被不能删除了，因为删除会影响到其他字符串。实在需要删除字符串的可以使用Counting bloomfilter(CBF)，这是一种基本Bloom Filter的变体，CBF将基本Bloom Filter每一个Bit改为一个计数器，这样就可以实现删除字符串的功能了。
### 应用
Bloom-Filter一般用于在大数据量的集合中判定某元素是否存在。例如邮件服务器中的垃圾邮件过滤器。在搜索引擎领域，Bloom-Filter最常用于网络蜘蛛(Spider)的URL过滤，网络蜘蛛通常有一个URL列表，保存着将要下载和已经下载的网页的URL，网络蜘蛛下载了一个网页，从网页中提取到新的URL后，需要判断该URL是否已经存在于列表中。此时，Bloom-Filter算法是最好的选择。
## cuckoo filter
Bloom Filter 可能存在flase positive，并且无法删除元素，而Cuckoo filter就是解决这两个问题的。
### 基本思想
cuckoo filter使用两个hash函数和两个分离的表，在表中以K-V的形式记录hash的结果键值对，例如key x and value a -> `<x,a>`
### 用法
**插入元素**
对于一个要插入的元素x(key x)，使用hash1()和hash2()分别计算出在两个表T1、T2中的插入位置，当两个表中的位置都为empty时，直接插入，若出现冲突，会出现以下两种情况：
**1.一个位置冲突，另一个位置empty**
此时，对于冲突的位置不要插入，对于empty的位置则插入，如图所示：
![](https://i.imgur.com/3kFCw70.jpg)

**2.两个位置都冲突**
若两个位置都发生冲突，则随机选择一个位置，并将原有元素踢出，但原有元素踢出后不能抛弃，应利用另一个hash函数检测该踢出元素是否在另一个表中存在，如在另一个表中也发生冲突，则再次踢出。如此递归下去，直到不发生冲突为止。流程大致如下：
![](https://i.imgur.com/FUtazDC.jpg)

但值得注意的是，这样替换可能成环，造成无限循环的情形。因此需要规定一个最大的递归数。

**检查元素是否存在**
检查元素存在时，利用hash1和hash2两个函数进行hash操作，只要有一个hash函数在表中命中，即说明该元素已经出现过。
### 改进
Cockoo hashing 有两种变形：一种通过增加哈希函数进一步提高空间利用率；另一种是增加哈希表，每个哈希函数对应一个哈希表，每次选择多个张表中空余位置进行放置，三个哈希表可以达到80% 的空间利用率。

Cockoo hashing 的过程可能因为反复踢出无限循环下去，这时候就需要进行一次循环踢出的限制，超过限制则认为需要添加新的哈希函数。

---
### 参考文献
[1]海量数据处理算法—Bloom Filter, https://blog.csdn.net/hguisu/article/details/7866173
[2] BloomFilter——大规模数据处理利器, http://www.cnblogs.com/heaad/archive/2011/01/02/1924195.html
[3] Wikipedia. Bloom filter. http://en.wikipedia.org/wiki/Bloom_filter
[4] cuckoo hash, http://codecapsule.com/2013/07/20/cuckoo-hashing/
