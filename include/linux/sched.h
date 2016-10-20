#ifndef _SCHED_H     // schedule 所以简写为 sched.h 
#define _SCHED_H

#define NR_TASKS 64                     // 系统中同时最多任务(进程)数
#define HZ 100                              // 定义系统时钟滴答频率(1百赫兹，每个滴答10ms)

#define FIRST_TASK task[0]                     // 任务0比较特殊，所以特意给他单独定义一个符号
#define LAST_TASK task[NR_TASKS-1]     // 任务数组中的最后一项任务

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <signal.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

// 这里定义了进程运行可能处的状态
#define TASK_RUNNING		     0            // 进程正在运行或已准备就绪
#define TASK_INTERRUPTIBLE	  1            // 进程处于可中断等待状态
#define TASK_UNINTERRUPTIBLE	2            // 进程处于不可中断等待状态，主要用于 I/O 操作等待
#define TASK_ZOMBIE		       3            // 进程处于僵死状态，已经停止运行，但父进程还没发信号
#define TASK_STOPPED		      4           // 进程已停止

#ifndef NULL
#define NULL ((void *) 0)                       // 定义NULL为空指针
#endif

// 复制进程的页目录页表，Linus 认为这是内核中最复杂的函数之一 (mm/memory.c 105)
extern int copy_page_tables(unsigned long from, unsigned long to, long size);
// 释放页表所指定的内存块及页表本身 (mm/memory.c 150)
extern int free_page_tables(unsigned long from, unsigned long size);

extern void sched_init(void);    // 调度程序的初始化函数  (kernel/sched.c 385)
extern void schedule(void);      // 进程调度函数  (kernel/sched, 104)
extern void trap_init(void);       // 异常(陷阱)中断处理初始化函数，设置中断调用门允许中断请求信号 (kernel/traps.c 181)
#ifndef PANIC
void panic(const char * str);       // 显示内核出错信息，然后进入死循环 (kernel/panic.c 16)
#endif
extern int tty_write(unsigned minor,char * buf,int count);    // 往 tty 上写指定长度的字符串 (kernel/chr_drv/tty_io.c 290)

typedef int (*fn_ptr)();      // 定义函数指针类型

// 下面是数学协处理器使用的结构，主要用于保存进程切换时 i387 的执行状态信息
struct i387_struct {        
	long	cwd;             // 控制字 Control word  
	long	swd;             // 状态字 Status word 
	long	twd;              // 标记字 Tag word 
	long	fip;               // 协处理器代码指针
	long	fcs;               // 协处理器代码段寄存器
	long	foo;              // 内存操作数的偏移位置
	long	fos;               // 内存操作数的段值
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */  // 8个10字节的协处理累加器
};

// 任务状态段数据结构    p332  图 8-14
struct tss_struct {
	long	back_link;	/* 16 high bits zero */
	long	esp0;
	long	ss0;		/* 16 high bits zero */
	long	esp1;
	long	ss1;		/* 16 high bits zero */
	long	esp2;
	long	ss2;		/* 16 high bits zero */
	long	cr3;
	long	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long	esp;
	long	ebp;
	long	esi;
	long	edi;
	long	es;		/* 16 high bits zero */
	long	cs;		/* 16 high bits zero */
	long	ss;		/* 16 high bits zero */
	long	ds;		/* 16 high bits zero */
	long	fs;		/* 16 high bits zero */
	long	gs;		/* 16 high bits zero */
	long	ldt;		/* 16 high bits zero */
	long	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
	struct i387_struct i387;
};

// 进程描述符
struct task_struct {
/* these are hardcoded - don't touch */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */             // 任务的运行状态 (-1不可运行，0可运行(就绪)，>0已停止)
	long counter;                                                                            // 任务运行时间计数(递减)(滴答数)，运行时间片
	long priority;                                                                             // 运行优先数，任务开始运行时 counter = priority,越大运行越长
	long signal;                                                                              // 信号，是位图，每个比特位代表一种信号，信号值=位偏移值+1 
	struct sigaction sigaction[32];                                                     // 信号执行属性结构，对应信号将要执行的操作和标志信息
	long blocked;	/* bitmap of masked signals */                            // 进程信号屏蔽码(对应信号位图)
/* various fields */
	int exit_code;                                                                             // 任务执行停止的退出码，其父进程会取
	unsigned long start_code,end_code,end_data,brk,start_stack;      // 代码段地址，代码长度(字节数)，代码长度+数据长度(字节数)，总长度(字节数)，堆栈段地址
	long pid,father,pgrp,session,leader;                                            // 进程标识号，父进程号，进程组号，会话号，会话首领
	unsigned short uid,euid,suid;                                                     // 用户标识号(用户id),有效用户id,保存的用户id 
	unsigned short gid,egid,sgid;                                                     // 组标识号(组id),有效组id,保存的组id 
	long alarm;                                                                                // 报警定时值
	long utime,stime,cutime,cstime,start_time;                                    // 用户态运行时间 (滴答数),系统态运行时间(滴答数),子进程用户态运行时间(滴答数),子进程系统态运行时间，进程开始运行时刻
	unsigned short used_math;                                                        // 标志:是否使用了协处理器
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */                         // 进程使用 tty 的子设备号， -1 表示没有使用
	unsigned short umask;                                                              // 文件创建属性屏蔽位
	struct m_inode * pwd;                                                                 // 当前工作目录 i 节点结构,用于解析相对路径名
	struct m_inode * root;                                                                  // 根目录 i 节点结构，用于解析绝对路径名
	struct m_inode * executable;                                                        // 执行文件 i 节点结构
	unsigned long close_on_exec;                                                      // 执行时关闭文件句柄位图标志   include/fcntl.h
	struct file * filp[NR_OPEN];                                                            // 进程使用的文件表结构
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];                                                               // ldt 本任务的局部表描述符  0-空， 1-代码段 cs, 2-数据和堆栈段 ds&ss
/* tss for this task */
	struct tss_struct tss;                                                                      // tss 本进程的任务状态段信息结构 
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 */
// state=0(TASK_RUNNING)表示可以运行(就绪)
// counter=15 任务运行的时间片(150ms),开始的时候与 priority 的值相等
// priority=15 运行的优先权
// signal=0 表示没有任务信号
// sigaction[32]={{},} 信号向量表为空 
// blocked=0 表示不阻塞任何信号
// exit_code=0 
// start_code=0 代码段起始地址
// end_code=0  代码段的长度
// end_data=0  数据段的长度
// brk=0
// start_stack=0 表示还没有分配栈
// pid=0  0号进程
// father=-1 父进程号(表示没有父进程)
// pgrp=0 父进程组号
// session=0  
// leader=0
// uid=0     用户标识符(用户id)
// euid=0   有效用户id
// suid=0    保存的用户id
// gid=0     组标识号(组id)
// egid=0   有效组id
// sgid=0   保存的组id
// alarm=0         报警定时值(没有设定)
// utime=0         用户态时间
// stime=0         系统态时间
// cutime=0       子进程用户态时间
// cstime=0       子进程系统态运行时间
// start_time=0  进程开始运行时刻
// used_math=0  是否使用了协处理器 
// tty=-1                   进程没有使用tty
// umask=0022         文件创建属性屏蔽位
// pwd=NULL             当前工作目录i节点
// root=NULL              当前工作目录i节点
// executable=NULL   执行文件i节点
// close_on_exec=0    执行时关闭文件句柄位图标志
// filp[20]={NULL}      进程使用的文件表结构
// ldt[3]
// 代码长 640K, 基址0x0, G=1, D=1, DPL=3, P=1, TYPE=0x0a 
// 数据长 640K, 基址0x0, G=1, D=1, DPL=3, P=1, TYPE=0x02 
// tss 

// 初始化进程是一切进程的祖先进程，是通过"硬编码"写到程序中的，初始进程代表的是内核的进程，但是它不会被调度程序调度
// 它的目的倒不是为了让内核运行，而是为其他的进程提供一个复制的基点，故task_struct结构是直接用代码写成的的，在为其分配空间的时候，值就设定好了
#define INIT_TASK \
/* state etc */	{ 0,15,15, \                                                                     
/* signals */	0,{{},},0, \                                                                        
/* ec,brk... */	0,0,0,0,0,0, \                                                                  
/* pid etc.. */	0,-1,0,0,0, \                                                                   
/* uid etc */	0,0,0,0,0,0, \                                                                  
/* alarm */	0,0,0,0,0,0, \                                                                      
/* math */	0, \                                                                                    
/* fs info */	-1,0022,NULL,NULL,NULL,0, \                                          
/* filp */	{NULL,}, \                                                                               
	{ \                                                                                                    
		{0,0}, \
/* ldt */	{0x9f,0xc0fa00}, \                                                                  
		{0x9f,0xc0f200}, \                                                                         
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\        
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
}

extern struct task_struct *task[NR_TASKS];              // 任务指针数组  NR_TASKS=64          
extern struct task_struct *last_task_used_math;       // 上一个使用过协处理器的进程
extern struct task_struct *current;                          // 当前进程结构指针变量
extern long volatile jiffies;                                      // 从开机开始算起的滴答数(10ms/滴答)
extern long startup_time;                                      // 开机时间，从 1970:0:0:0 开始计算时的秒数

#define CURRENT_TIME (startup_time+jiffies/HZ)    // 当前时间(秒数)

extern void add_timer(long jiffies, void (*fn)(void));                  // 添加定时器函数 (定时时间 jiffies 滴答数，定时到时调用函数*fn())   kernel/sched.c 
extern void sleep_on(struct task_struct ** p);                            // 不可中断的等待睡眠 kernel/sched.c 151 
extern void interruptible_sleep_on(struct task_struct ** p);       // 可中断的等待睡眠 kernel/sched.c 167 
extern void wake_up(struct task_struct ** p);                            // 明确唤醒睡眠的进程  

/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
/*
在 GDT 表中寻找第一个TSS的入口，0-没有用nul,1-代码段cs,2-数据段ds,3-系统调用syscall,4-任务状态段TSS0,5-局部表LTD0,6-任务状态TSS1 等等 
*/
// 从英文注释可以猜想到，Linus当时想把系统调用的代码专门放在 GDT 表中第 4 个独立的段中，但后来并没有那样做，于是就一直把 GDT 表中第 4 个描述符项  
// 上面 syscall 项 闲置在一旁

/*
0     |         NULL      |
1     | 系统代码段 CS |
2     | 系统数据段 DS |
3     |    系统调用      |
4     |        TSS0       |         <-- FIRST_TSS_ENTRY    4
5     |        LDT0      |         <-- FIRST_LDT_ENTRY    5
6     |        TSS1       |
7     |        LDT1      |
       |                      |
       |                      |
254 |      TSS126     |
255 |      LDT126    |

一段是8字节 8*8=64位 256*8 = 2048 = 2KB

*/

#define FIRST_TSS_ENTRY 4                                                                   // 全局表中第一个任务状态段 TSS 描述符的选择索引号      4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)                                    // 全局表中第一个局部描述符表(LDT)描述符的选择索引表   5
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))         // 进程号同相应的 TSS 转换公式: FIRST_TSS_ENTRY*8字节+n*16字节
// n=0:  4*8+0*16= 32
// n=1:  4*8+1*16= 32+16=38
// 计算在全局表中第 n 个任务的 TSS 端描述符的选择符值(偏移量) 
// 因每个描述符占8字节，因此 FIRST_TSS_ENTRY<<3 ( FIRST_TSS_ENTRY * 8 )表示该描述符在 GDT 表中的起始偏移位置
// 因为每个任务使用 1 个 TSS 和 1 个 LDT 描述符，工占用 16 字节，因此需要 n <<4 ( * 16 ) 来表示对应 TSS 起始位置

#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))       // 进程号同相应的 LDT 转换公式: FIRST_LDT_ENTRY*8字节+n*16字节 
#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))                                        // load tss register                把第 n 号进程的 TSS 段选择符加载到任务寄存器 TR 中
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))                                    // load local descriptor table 把第 n 号进程的 LDT 段选择符加载到局部描述符表寄存器 LDTR 中 

// 取当前运行任务的任务号 (是任务数组中的索引值，与进程号 pid 不同)
// 返回: n - 当前任务号，用于 kernel/traps.c   

// 将任务寄存器中 TSS 段的选择符复制到 ax 中
// (eax - FIRST_TSS_ENTRY*8) --> eax 
// (eax/16)-->eax  = 当前任务号
#define str(n) \
__asm__("str %%ax\n\t" \                         
	"subl %2,%%eax\n\t" \                         
	"shrl $4,%%eax" \                                
	:"=a" (n) \
	:"a" (0),"i" (FIRST_TSS_ENTRY<<3))
/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * tha math co-processor latest.
 */
#define switch_to(n) {\
struct {long a,b;} __tmp; \
__asm__("cmpl %%ecx,current\n\t" \
	"je 1f\n\t" \
	"movw %%dx,%1\n\t" \
	"xchgl %%ecx,current\n\t" \
	"ljmp *%0\n\t" \
	"cmpl %%ecx,last_task_used_math\n\t" \
	"jne 1f\n\t" \
	"clts\n" \
	"1:" \
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
	"d" (_TSS(n)),"c" ((long) task[n])); \
}


#define PAGE_ALIGN(n) (((n)+0xfff)&0xfffff000)      // 页面地址对准(在内核代码中没有任何地方引用)

#define _set_base(addr,base)  \
__asm__ ("push %%edx\n\t" \
	"movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %%dl,%1\n\t" \
	"movb %%dh,%2\n\t" \
	"pop %%edx" \
	::"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7)), \
	 "d" (base) \
	)

#define _set_limit(addr,limit) \
__asm__ ("push %%edx\n\t" \
	"movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %1,%%dh\n\t" \
	"andb $0xf0,%%dh\n\t" \
	"orb %%dh,%%dl\n\t" \
	"movb %%dl,%1\n\t" \
	"pop %%edx" \
	::"m" (*(addr)), \
	 "m" (*((addr)+6)), \
	 "d" (limit) \
	)

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , (base) )              // 设置局部描述符表中 ldt 描述符的基地址字段
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )   // 设置局部描述符表中 ldt 描述符的段长字段

/**
#define _get_base(addr) ({\
unsigned long __base; \
__asm__("movb %3,%%dh\n\t" \
	"movb %2,%%dl\n\t" \
	"shll $16,%%edx\n\t" \
	"movw %1,%%dx" \
	:"=d" (__base) \
	:"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7)) \
        :"memory"); \
__base;})
**/

// 从地址 addr 处描述符中取段基地址，功能与 _set_base() 正好相反
// edx - 存放基地址(__base); %1 - 地址 addr 偏移 2; %2 - 地址 addr 偏移4; %3 - addr 偏移 7 
static inline unsigned long _get_base(char * addr)
{
         unsigned long __base;
         __asm__("movb %3,%%dh\n\t"         // 取 [addr+7]处基址高16位的高8位(位31-24)-->dh 
                 "movb %2,%%dl\n\t"                // 取 [addr+4]处基址高16位的低8位(位23-16)-->dl
                 "shll $16,%%edx\n\t"              // 基地址高16位移到 edx 中高 16 位处
                 "movw %1,%%dx"                    // 取 [addr+2] 处基地址低 16 位(位 15-0)-->dx 
                 :"=&d" (__base)                     // 从而 edx 中含有 32 位的段基地址 
                 :"m" (*((addr)+2)),
                  "m" (*((addr)+4)),
                  "m" (*((addr)+7)));
         return __base;
}

#define get_base(ldt) _get_base( ((char *)&(ldt)) )       // 取局部描述符表中 ldt 所指段描述符的基地址

// 取段选择符 segment 指定的描述符中的段限长值 
// 指令 lsl 是 Load Segment Limit 缩写，它从指定段描述符中取出分散的限长比特位拼成完整的段限长值放入指定寄存器中，所得的段限长是实际字节数减1
// 因此这里还需要加 1 后才能返回
// %0 - 存放段长值(字节数): %1 - 段选择符 segment 

#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

#endif
