## 宏定义
### 1 _GNU_SOURCE
在进行io编程时，我们经常想绕过OS的page cache层，得到更真实的io测试结果，这就需要在打开文件上增加O_DIRECT标签：
```
fd = open(pathname, O_RDWR|O_DIRECT|O_SYNC);
```
但这样进行编译会报错“O_DIRECT未定义”。此时在文件头加上宏定义，即可解决问题
```
#define _GNU_SOURCE
```
那么这个宏定义究竟有何作用呢？
>Defining _GNU_SOURCE has nothing to do with license and everything to do with writing (non-)portable code. If you define _GNU_SOURCE, you will get:

>1 access to lots of nonstandard GNU/Linux extension functions
>2 access to traditional functions which were omitted from the POSIX standard (often for good reason, such as being replaced with better alternatives, or being tied to particular legacy implementations)
>3 access to low-level functions that cannot be portable, but that you sometimes need for implementing system  utilities like mount, ifconfig, etc.
>4 broken behavior for lots of POSIX-specified functions, where the GNU folks disagreed with the standards committee on how the functions should behave and decided to do their own thing.

>As long as you're aware of these things, it should not be a problem to define _GNU_SOURCE, but you should avoid >defining it and instead define _POSIX_C_SOURCE=200809L or _XOPEN_SOURCE=700 when possible to ensure that your >programs are portable.

>In particular, the things from _GNU_SOURCE that you should never use are #2 and #4 above.

在gnu关于macro的说明中，可以找到：
>If you define this macro, everything is included: ISO C89, ISO C99, POSIX.1, POSIX.2, BSD, SVID, X/Open, LFS, and GNU extensions. In the cases where POSIX.1 conflicts with BSD, the POSIX definitions take precedence.

这说明，在使用_GNU_SOURCE之后，我们几乎可以调用所有的posix和接口和新旧版本的C语言接口。

---
###参考文献
[1]https://stackoverflow.com/questions/5582211/what-does-define-gnu-source-imply

[2]https://www.gnu.org/software/libc/manual/html_node/Feature-Test-Macros.html