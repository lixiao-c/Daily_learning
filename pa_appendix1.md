## PA(programming assignment) Appendix1
---
## 目录
### [1 union结构](##1-union结构)
### [2 C语言结构体中冒号的用法](##2-C语言结构体中冒号的用法)
### [3 C语言移位操作](##3-C语言移位操作)
## 1 union结构
union和struct都是C语言中重要的结构。相比union，struct在日常中使用的更加频繁，但union也并非一无是处。从占据空间上来说，struct中的成员锁占据的空间是独立的，而union则是成员共用一块空间。在pa实现寄存器时，这一点非常有用。举例而言，eax与ax、ah、al复用空间。我们的实现如下。
```cpp
	union{ 	
		union {
			uint32_t _32;
			uint16_t _16;
			uint8_t _8[2];
		} gpr[8];

		struct{uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;};
	};
```
## 2 C语言结构体中冒号的用法
C语言中的冒号一般是三目运算符，如下面的条件表达式
```cpp
int a,b,c;
a=3;
b=2;
c=a>b?a:b
```
但在结构图中，冒号还有另一种用途，表示**位域**。有时候我们声明了一个变量，却只想让该变量占据几个bit位，这时候就需要位域了。
```cpp
  struct   bs   
  {   
  　int   a:8;   
  　int   b:2;   
  　int   c:6;   
  };    
```
上面这段代码，a占8位，b占2位，c占6位。利用位域，我们在pa中实现了eflags。设置位域时也可以不加变量名(匿名变量)：
```cpp
union Eflags{
		struct{
			uint32_t cf:1;
			uint32_t   :1;
			uint32_t pf:1;
			uint32_t   :3;
			uint32_t zf:1;
			uint32_t sf:1;
			uint32_t   :1;
			uint32_t If:1;
			uint32_t df:1;
			uint32_t of:1;
			uint32_t  :20;	
		};
		uint32_t eflags_val;
	} eflags;
```
## 3 C语言移位操作
在实现符号扩展时，使用了C语言的移位操作。从短数据类型扩展为长数据类型可以直接进行符号扩展。在pa中实现的是短数据类型是有符号数，因此要根据符号决定扩展位的填充。在C语言中，其右移是算数右移(会保持符号位不变)。于是我们可以通过两次移位操作，将8位的1001，扩展位16位的11111001。

对于无符号来说，右移是逻辑右移，左边总是补0。举一个编程实例
```
int a=0x00008001
int b=a<<16;
b>>=16;
unsigned int c=a<<16;
c>>=16;
```
此时b为0xffff8001，c为0x00008001。