# 南京大学ICS课程PA(programming assignment) PART1
---
## 目录
### [0.做本课程的初衷](##0.做本课程的初衷)
### [1 pa简介](##1-pa简介)
#### [1.1 pa的有关链接](###1.1-pa的有关链接)
#### [1.2 pa实验环境](###1.2-pa实验环境)
### [2 PA1](## 2-PA1)
####[2.1 PA的架构](### 2.1-PA的架构)
####[2.2 第三视角](### 2.2-第三视角)
####[2.3 武功秘籍](### 2.3-武功秘籍)
## 0.做本课程的初衷
本科阶段做过的课程设计不算多，操作系统、编译原理各算一个，但其他课程的课设都有些潦草。国外大学有不少有趣的课程设计，较著名的有MIT 6.828的JOS，cmu 15-440分布式系统，MIT 6.830数据库原理等等。相比之下，国内有趣的课程设计比较少，南大的PA算是为数不多的好课设。
研究生选择在系统方向继续学习，再加上最近做的东西与qemu相关，南大的pa基于qemu实现，于是选择南大的PA继续熟悉计算机系统的运作。一可以实际体验编写系统的快感，二是看一看其他学校本科教学的内容。
## 1 pa简介
理解"程序如何在计算机上运行"的根本途径是从"零"开始实现一个完整的计算机系统。 南京大学计算机科学与技术系计算机系统基础课程的小型项目 (Programming Assignment, PA)将提出x86架构的一个教学版子集n86, 指导学生实现一个功能完备的n86模拟器NEMU(NJU EMUlator), 最终在NEMU上运行游戏"仙剑奇侠传", 来让学生探究"程序在计算机上运行"的基本原理。
### 1.1 pa的有关链接
我选择了ics_pa2016来做(实际上只在github上找到了2016版)。
pa2016的代码：
https://github.com/NJU-ProjectN/ics2016
pa2016的实验说明（国内访问gitbook真的慢）：
https://nju-ics.gitbooks.io/ics2016-programming-assignment/content/
我所做的pa代码：
https://github.com/lixiao-c/NJU_icspa2016
### 1.2 pa实验环境
PA需要以下环境(工具)：
1.linux操作系统（推荐Debian）
2.qemu-system-x86
3.gcc
4.gdb
5.vim
要求学生可以使用虚拟机(windows下VMware、virtualBox)或者容器(docker)，在上边安装系统。设计PA的几位学长为了本科教学也是煞费苦心，要求安装无图形界面的的Debian，编辑器使用vim，用git管理相关代码。
## 2 PA1
pa0主要是一些环境的安装和熟悉，里边有一些工具和相关知识的介绍链接，可以跳过。周末实现了PA1的内容，主要是为pa准备了monitor工具。
### 2.1 pa的架构
![](https://i.imgur.com/IPoJkWG.png)
NEMU主要由4个模块构成: monitor, CPU, 存储管理, 设备, 它们依次作为4个PA关注的主题. 其中, CPU, 存储管理, 设备这3个模块共同组成一个虚拟的计算机系统, 程序可以在其上运行; monitor位于这个虚拟计算机系统之外, 主要用于监视这个虚拟计算机系统是否正确运行. monitor虽然不属于虚拟计算机系统, 但对PA来说, 它是必要的. 它除了负责与GNU/Linux进行交互(例如读写文件)之外, 还带有调试器的功能。

### 2.2 第三视角
本部分要求实现一个简单的调试器(类似gdb)。其中打印栈帧在本次lab还无法完成，其他均可完成。
![](https://i.imgur.com/67jJ3Rh.png)
### 2.2.1 开始运行pa
开始运行pa时，系统会在测试寄存器时assert，因为我们还没实现cpu的寄存器。对于i386架构，有25个寄存器，其中一大部分还是复用的，如eax与ah、al,所以只需要8*32大小内存即可。通过union结构实现的内存复用，便可以实现寄存器相关功能。
```
typedef struct {
	union{ 	
		union {
			uint32_t _32;
			uint16_t _16;
			uint8_t _8[2];
		} gpr[8];

	/* Do NOT change the order of the GPRs' definitions. */

		struct{uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;};
	};

	swaddr_t eip;

} CPU_state;
```
### 2.2.2 基本招式
实现打印寄存器、打印内存、单步调试。首先我们可以看模拟器是如何与用户交互的(/nemu/src/monitor/debug/ui.c)
main.c中，在执行完基本的初始化后，会执行ui_loop函数，其在ui.c中。这是一个while(true)的循环，会接受用户的输入，执行相应的操作(handle函数)，当handle函数return -1时，nemu便停止运行了。实现的新功能只需要添加到handle列表中，便可以运行了。
#### 打印寄存器 info r
没啥好说的，info_watchpoint会在下文介绍。
```
static int cmd_info(char *args){
	if(args[0]=='w')
		info_watchpoint();
	else if(args[0]=='r'){
		printf("REG	VALUE\n");
		printf("eax	0x%x\n",reg_l(R_EAX));	
		printf("ecx	0x%x\n",reg_l(R_ECX));	
		printf("ebx	0x%x\n",reg_l(R_EDX));	
		printf("edx	0x%x\n",reg_l(R_EBX));	
		printf("esp	0x%x\n",reg_l(R_ESP));
		printf("ebp	0x%x\n",reg_l(R_EBP));	
		printf("esi	0x%x\n",reg_l(R_ESI));	
		printf("edi	0x%x\n",reg_l(R_EDI));	
		printf("eip	0x%x\n",cpu.eip);		
	}
	return 0;
}
```
#### 打印内存 x N expr
这里的expr函数计算expr的值，会在下面介绍。利用swaddr_read可以将相应地址的值读出。
```
static int cmd_x(char *args){
	int i=0;
	int len=strlen(args);
	while(i<len){	
		if(args[i]==' ')
			break;
		i++;
	}
	if(i==len){
		printf("please input again\n");
		return 0;
	}
	char str1[10],str2[32];
	int j;	
	for(j=0;j<i;j++)
		str1[j]=args[j];
	str1[i]='\0';
	for(j=0;j<len-i-1;j++)
		str2[j]=args[i+1+j];
	str2[len]='\0';
	bool* success=malloc(1);	
	int num=expr(str1,success);
	int addr=expr(str2,success);
	printf("addr		value\n");
	for(i=0;i<num;i++)
		printf("0x%x		0x%x\n",addr+i*4,swaddr_read(addr+i*4,4));
	return 0;
}
```
### 2.2.3 有求必应
这部分是让我比较兴奋的一部分，要求计算表达式的值，也就是上文的expr函数。进一步说就是简单编译器的模型。主要是词法分析和语法分析两部分。
#### 词法分析
需要实现的表达式计算主要是+、-、*、/以及 $+reg、0x+addr这几种。运算还有==、！=、&&、||、！、*(取值)。
可以得到以下正则式规则。
```
enum {
	NOTYPE = 256, EQ, UEQ, AND, OR, NUM, HNUM, REG
};
static struct rule {
	char *regex;
	int token_type;
} rules[] = {
	{"[0][x]([0-9,a-f,A-F]){1,8}",HNUM},
	{"[$][a-z,A-Z]+",REG},	
	{"[0-9]+",NUM},	
	{"\\)",')'},
	{"\\(",'('},	
	{"/",'/'},	
	{"\\*",'*'},	
	{"-", '-'},
	{" +",	NOTYPE},				// spaces
	{"\\+", '+'},					// plus
	{"==", EQ},					// equal
	{"!=",UEQ},
	{"!",'!'},
	{"[&][&]",AND},
	{"[|][|]",OR}
};
```
词法分析时，会遍历规则进行相应匹配，如果匹配成功就将其存到Token结构中，该结构记录token的类型，必要时记录token的值(num、hnum、reg等)。
#### 语法分析   
语法分析主要利用递归思想，大致框架如下。其中check_parentheses检查是否表达式被括号包裹。
```
eval(p, q) {
    if(p > q) {
        /* Bad expression */
    }
    else if(p == q) { 
        /* Single token.
         * For now this token should be a number. 
         * Return the value of the number.
         */ 
    }
    else if(check_parentheses(p, q) == true) {
        /* The expression is surrounded by a matched pair of parentheses. 
         * If that is the case, just throw away the parentheses.
         */
        return eval(p + 1, q - 1); 
    }
    else {
        /* We should do more things here. */
		 op = the position of dominant operator in the token expression;
        val1 = eval(p, op - 1);
        val2 = eval(op + 1, q);

        switch(op_type) {
            case '+': return val1 + val2;
            case '-': /* ... */
            case '*': /* ... */
            case '/': /* ... */
            default: assert(0);
        }
    }
}
```
根据运算符的优先级，可以通过该架构构建我们的语法分析机制。其BNF(产生式)如下,代码较多，具体实现代码参见/nemu/src/monitor/debug/expr.c：
```
<expr> ::= <decimal-number>
    | <hexadecimal-number>    # 以"0x"开头
    | <reg_name>            # 以"$"开头
    | "(" <expr> ")"
    | <expr> "+" <expr>
    | <expr> "-" <expr>
    | <expr> "*" <expr>
    | <expr> "/" <expr>
    | <expr> "==" <expr>
    | <expr> "!=" <expr>
    | <expr> "&&" <expr>
    | <expr> "||" <expr>
    | "!" <expr>
    | "*" <expr>            # 指针解引用
```
### 2.2.4 天网恢恢
本阶段实现了监视点(watchpoint)机制。类似gdb中的breakpoint的实现，我们可以设置、删除、查看监视点。
#### watchpoint的原理
首先，我们需要一个结构来存储监视点。这个结构WP保存在nemu/include/monitor/watchpoint.h。系统默认最多设置32个监视点，使用两个链表记录监视点状态(free or active)。首先要实现创建和删除监视点的函数new_wp和free_wp，主要是链表的操作，代码参见nemu/src/monitor/watchpoint.c。
其次，现在监视点的结构中记录的内容过少，只用no(编号)和next(你懂得)。为了实现，监视点记录的内容变化时，系统就暂停的目标，我们需要增加两个变量，记录监视点的expr及其值：
```
typedef struct watchpoint {
	int NO;
	struct watchpoint *next;
	
	/* TODO: Add more members if necessary */
	char expr[32];
	uint32_t expr_record_val;

} WP
```
#### 起作用的watchpoint
watchpoint的作用在于可以让程序停下来，所以在cpu执行每一次，都要判断一下监视点对应的expr是否发生了变化。在/nemu/src/monitor/cpu-exec.c,添加如下代码。其中check_watchpoint会检查active的监视点是否发生变化。如变化，nemu的状态就变为STOP，等待用户的下一步命令。
```
		/* TODO: check watchpoints here. */
		if(!check_watchpoint())	{		
			nemu_state = STOP;
			ui_mainloop();	
		}	
```
同时，在ui部分加入一系列控制函数，如cmd_w（添加监视点）、cmd_d（删除监视点）、cmd_info（打印监视点信息）。
#### watchpoint与breakpoint
断点的功能是让程序暂停下来, 从而方便查看程序某一时刻的状态. 事实上, 我们可以很容易地用监视点来模拟断点的功能:
```
w $eip ！= ADDR
```
其中 ADDR 为设置断点的地址. 这样程序执行到 ADDR 的位置时就会暂停下来。
### 2.3 武功秘籍
本部分主要强调了i386手册(武功秘籍)的重要性，是计算机系统的武功秘籍。i386手册地址如下：
http://www.logix.cz/michal/doc/i386/manual.htm
一起IA_32的开发者手册(操作系统经常用到)：
https://software.intel.com/en-us/articles/intel-sdm?iid=tech_vt_tech+64-32_manuals

---
本部分到此结束！