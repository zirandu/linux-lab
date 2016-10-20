/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__      // 定义宏是为了包括定义在 unistd.h 中的内嵌汇编代码等信息
#include <unistd.h>
#include <time.h>

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
static inline int fork(void) __attribute__((always_inline));        // system_call.s  sys_fork --> fork.c copy_process
static inline int pause(void) __attribute__((always_inline));
static inline _syscall0(int,fork)                                   // system_call.s  sys_fork --> fork.c copy_process 
static inline _syscall0(int,pause)                                 // int pause() 系统调用: 暂停进程的执行，知道收到一个信号
static inline _syscall1(int,setup,void *,BIOS)                // int setup(void * BIOS)系统调用，仅用于 linux 初始化(仅在这个程序中被调用)
static inline _syscall0(int,sync)                                   // int sync() 系统调用: 更新文件系统

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/fs.h>

static char printbuf[1024];          // 静态字符串数组，用作内核显示信息的缓存

extern int vsprintf();                   // 送格式化输出到一字符串中  vsprintf.c 
extern void init(void);
extern void blk_dev_init(void);
extern void chr_dev_init(void);
extern void hd_init(void);
extern void floppy_init(void);
extern void mem_init(long start, long end);
extern long rd_init(long mem_start, int length);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;

/*
 * This is set up by the setup-routine at boot-time
 */
#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;
	startup_time = kernel_mktime(&time);
}

static long memory_end = 0;
static long buffer_memory_end = 0;
static long main_memory_start = 0;

struct drive_info { char dummy[32]; } drive_info;

/*
idle进程(进程0别称)
进程0(任务0):     系统初始化  -->  对物理内存各部分进行功能划分与分配 -->  系统各部分初始化，包括对任务 0 初始化  -->  移到任务 0 中执行  -->  创建进程 1 (init)  --> 空闲时执行 pause() 
                                                                                                                                                                                                                                |
                                  -------------------------------------------------------------------------------------------------------------
                                  |
进程1(init进程):   加载根文件系统  -->   设置终端标准 IO  -->  创建进程 2 -->  循环等待进程 2 退出   -->  创建子进程   -->  循环等待进程退出   --     <----
                                                                                             |                                |                               |       |                                         |            |
                                  ----------------------------------         --------------                               <---------------<-----------             |
                                  |                                                                   |                                                               |                                                      |
进程2:               输入定向到 rc 文件    -->    执行 shell 处理 rc   -->   退出                                                            |                                                      |
                                                                                                                                                                      |                                                      |                                                         
                                                                                                                                                                      |                                                      |
进程n:                                                                                                                                                    设置终端标志IO  -->  执行 shell --> 退出 -->
*/

void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
    // 此时中断仍被禁止着，做完必要的设置后将其开启

 	ROOT_DEV = ORIG_ROOT_DEV;    // ROOT_DEV 定义在 fs/super.c 
 	drive_info = DRIVE_INFO;              // 复制 0x90080 处的硬盘参数表
	memory_end = (1<<20) + (EXT_MEM_K<<10);      // 开始内存的初始化   内存大小=1Mb+扩展内存(k)*1024字节
	memory_end &= 0xfffff000;                                    // 忽略不到 4Kb(1页)的内存数
	if (memory_end > 16*1024*1024)                          // 如果内存超过 16 Mb，按 16Mb 计
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024) 
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
	main_memory_start = buffer_memory_end;          // 主内存的起始位置=缓冲区末端
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
	mem_init(main_memory_start,memory_end);     // 主内存初始化  mm/memory.c  399
	trap_init();                                    // 陷阱门(硬件终端向量)   kernel/traps.c 181 
	blk_dev_init();                               // 块设备初始化              kernel/blk_drv/ll_rw_blk.c 157
	chr_dev_init();                               // 字符设备初始化(这里仅仅留了接口，没有实现)    kernel/chr_drv/tty_io.c   347  
	tty_init();                                      // 终端初始化         kernel/chr_drv/tty_io.c 105
	time_init();                                    // 启动时间初始化   startup_time 76
	sched_init();                                  // 调度程序初始化  加载任务0的 tr, ldtr     kernel/sched.c  385
	buffer_init(buffer_memory_end);    // 缓冲区初始化,建内存链表  fs/buffer.c  348
	hd_init();                                       // 硬盘初始化     kernel/blk_drv/hd.c  343
	floppy_init();                                 // 软盘初始化     kernel/blk_drv/floppy.c   457 
	sti();                                              // 开启终端   start interrupt 
	move_to_user_mode();                  // 将系统转入用户模式，以后内核要想工作的话也要用系统调用来完成了  include/asm/system.h
	if (!fork()) {		/* we count on this going ok */     // system_call.s  sys_fork --> fork.c copy_process  产生 1 号进程 
		init();       // 在新建的子进程(任务1)中执行
	}
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
	for(;;) pause();      // 0 号进程的工作到此结束，但是程序可不能退出，所以就循环吧
}

// 产生格式化信息并输出到标准输出设备 stdout(1),这里指屏幕上显示
static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));   // 将缓冲区的内容输出到 标准设备 1--stdout 
	va_end(args);
	return i;
}

// sh 的参数
// 读取并执行 /etc/rc 文件时所使用的命令行参数和环境参数 
static char * argv_rc[] = { "/bin/sh", NULL };      // 调用执行程序时参数的字符串数组
static char * envp_rc[] = { "HOME=/", NULL };    // 调用执行程序时的环境字符串输入

// 运行登录 shell 时所使用的命令行参数和环境参数
// "-" 是传递给 shell 程序 sh 的一个标志，通过识别该标志 sh 程序会作为登录 shell 执行
static char * argv[] = { "-/bin/sh",NULL };               // sh 程序会作为登录 shell 执行，其执行过程与在 shell 提示符执行 sh 不一样
static char * envp[] = { "HOME=/usr/root", NULL };

// init() 函数运行在任务 0 第 1 次创建的子进程(任务1)中，它首先对第一个将要执行的程序(shell)的环境进行初始化，然后以登录 shell 方式加载该程序并执行之
void init(void)
{
	int pid,i;

    // 用于读取硬盘参数包括分区表信息并加载虚拟盘(若存在的话)和安装根文件系统设备  kernel/blk_drv/hd.c 
	setup((void *) &drive_info);     // drive_info 结构中是 2 个硬盘参数表
    // 函数前面的 "(void)" 前缀用于表示强制函数无需返回值
	(void) open("/dev/tty0",O_RDWR,0);   // 读写访问方式打开设备 "/dev/tty0",它对应终端控制台，由于是第一次打开文件操作，因此产生的文件句柄号肯定是0，该句柄是系统默认的控制台标准输入句柄 stdin
	(void) dup(0);                                   // 复制上面的句柄，产生句柄 1 号--stdout 标准输出设备    
	(void) dup(0);                                   // 复制上面的句柄，产生句柄 2 号--stderr 标准出错输出设备
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS, NR_BUFFERS*BLOCK_SIZE); // 缓冲区块数和总字节数，每块 1024 字节
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);  // 住内存区空闲内存字节数

    // 产生一个用于运行 sh 的进程
    if (!(pid=fork())) 
    {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);          // 如果打开文件失败，则退出(lib/_exit.c, 10)
        // 运行可执行文件 sh,它是系统服务程序，位于文件系统盘中
		execve("/bin/sh",argv_rc,envp_rc);     // 替换成 /bin/sh 程序并执行shell程序，所携带的参数和环境变量分别由 argv_rc 和 envp_rc 数组给出
		_exit(2);                                           // 若 execve() 执行失败则退出 
	}

    // 还是父进程 1 执行的语句,wait() 等待子进程停止或终止，返回值应是子进程的进程号 pid 
    // 如果发现是 sh 执行结束，就向下执行，如果，是其他进程退出的话，由于进程号不同因此会继续循环等待
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;
	while (1)
    {
    		if ((pid=fork())<0) 
            {
    			printf("Fork failed in init\r\n");
    			continue;
    		}

    		if (!pid)
            {
    			close(0);close(1);close(2);       // 关闭 stdin, stdout, stderr 
    			setsid();                                 // 新建一个会话并设置进程组号
    			(void) open("/dev/tty0",O_RDWR,0);  // 重新打开 /dev/tty0 作为 stdin 
    			(void) dup(0);     // 复制成 stdout 
    			(void) dup(0);     // 复制成 stderr 
    			_exit(execve("/bin/sh",argv,envp));
    		}
    		while (1)
    			if (pid == wait(&i))
    				break;
    		printf("\n\rchild %d died with code %04x\n\r",pid,i);
    		sync();      // 同步操作，刷新缓冲区
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}
