/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__      // �������Ϊ�˰��������� unistd.h �е���Ƕ���������Ϣ
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
static inline _syscall0(int,pause)                                 // int pause() ϵͳ����: ��ͣ���̵�ִ�У�֪���յ�һ���ź�
static inline _syscall1(int,setup,void *,BIOS)                // int setup(void * BIOS)ϵͳ���ã������� linux ��ʼ��(������������б�����)
static inline _syscall0(int,sync)                                   // int sync() ϵͳ����: �����ļ�ϵͳ

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

static char printbuf[1024];          // ��̬�ַ������飬�����ں���ʾ��Ϣ�Ļ���

extern int vsprintf();                   // �͸�ʽ�������һ�ַ�����  vsprintf.c 
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
idle����(����0���)
����0(����0):     ϵͳ��ʼ��  -->  �������ڴ�����ֽ��й��ܻ�������� -->  ϵͳ�����ֳ�ʼ�������������� 0 ��ʼ��  -->  �Ƶ����� 0 ��ִ��  -->  �������� 1 (init)  --> ����ʱִ�� pause() 
                                                                                                                                                                                                                                |
                                  -------------------------------------------------------------------------------------------------------------
                                  |
����1(init����):   ���ظ��ļ�ϵͳ  -->   �����ն˱�׼ IO  -->  �������� 2 -->  ѭ���ȴ����� 2 �˳�   -->  �����ӽ���   -->  ѭ���ȴ������˳�   --     <----
                                                                                             |                                |                               |       |                                         |            |
                                  ----------------------------------         --------------                               <---------------<-----------             |
                                  |                                                                   |                                                               |                                                      |
����2:               ���붨�� rc �ļ�    -->    ִ�� shell ���� rc   -->   �˳�                                                            |                                                      |
                                                                                                                                                                      |                                                      |                                                         
                                                                                                                                                                      |                                                      |
����n:                                                                                                                                                    �����ն˱�־IO  -->  ִ�� shell --> �˳� -->
*/

void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
    // ��ʱ�ж��Ա���ֹ�ţ������Ҫ�����ú��俪��

 	ROOT_DEV = ORIG_ROOT_DEV;    // ROOT_DEV ������ fs/super.c 
 	drive_info = DRIVE_INFO;              // ���� 0x90080 ����Ӳ�̲�����
	memory_end = (1<<20) + (EXT_MEM_K<<10);      // ��ʼ�ڴ�ĳ�ʼ��   �ڴ��С=1Mb+��չ�ڴ�(k)*1024�ֽ�
	memory_end &= 0xfffff000;                                    // ���Բ��� 4Kb(1ҳ)���ڴ���
	if (memory_end > 16*1024*1024)                          // ����ڴ泬�� 16 Mb���� 16Mb ��
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024) 
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
	main_memory_start = buffer_memory_end;          // ���ڴ����ʼλ��=������ĩ��
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
	mem_init(main_memory_start,memory_end);     // ���ڴ��ʼ��  mm/memory.c  399
	trap_init();                                    // ������(Ӳ���ն�����)   kernel/traps.c 181 
	blk_dev_init();                               // ���豸��ʼ��              kernel/blk_drv/ll_rw_blk.c 157
	chr_dev_init();                               // �ַ��豸��ʼ��(����������˽ӿڣ�û��ʵ��)    kernel/chr_drv/tty_io.c   347  
	tty_init();                                      // �ն˳�ʼ��         kernel/chr_drv/tty_io.c 105
	time_init();                                    // ����ʱ���ʼ��   startup_time 76
	sched_init();                                  // ���ȳ����ʼ��  ��������0�� tr, ldtr     kernel/sched.c  385
	buffer_init(buffer_memory_end);    // ��������ʼ��,���ڴ�����  fs/buffer.c  348
	hd_init();                                       // Ӳ�̳�ʼ��     kernel/blk_drv/hd.c  343
	floppy_init();                                 // ���̳�ʼ��     kernel/blk_drv/floppy.c   457 
	sti();                                              // �����ն�   start interrupt 
	move_to_user_mode();                  // ��ϵͳת���û�ģʽ���Ժ��ں�Ҫ�빤���Ļ�ҲҪ��ϵͳ�����������  include/asm/system.h
	if (!fork()) {		/* we count on this going ok */     // system_call.s  sys_fork --> fork.c copy_process  ���� 1 �Ž��� 
		init();       // ���½����ӽ���(����1)��ִ��
	}
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
	for(;;) pause();      // 0 �Ž��̵Ĺ������˽��������ǳ���ɲ����˳������Ծ�ѭ����
}

// ������ʽ����Ϣ���������׼����豸 stdout(1),����ָ��Ļ����ʾ
static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));   // ������������������� ��׼�豸 1--stdout 
	va_end(args);
	return i;
}

// sh �Ĳ���
// ��ȡ��ִ�� /etc/rc �ļ�ʱ��ʹ�õ������в����ͻ������� 
static char * argv_rc[] = { "/bin/sh", NULL };      // ����ִ�г���ʱ�������ַ�������
static char * envp_rc[] = { "HOME=/", NULL };    // ����ִ�г���ʱ�Ļ����ַ�������

// ���е�¼ shell ʱ��ʹ�õ������в����ͻ�������
// "-" �Ǵ��ݸ� shell ���� sh ��һ����־��ͨ��ʶ��ñ�־ sh �������Ϊ��¼ shell ִ��
static char * argv[] = { "-/bin/sh",NULL };               // sh �������Ϊ��¼ shell ִ�У���ִ�й������� shell ��ʾ��ִ�� sh ��һ��
static char * envp[] = { "HOME=/usr/root", NULL };

// init() �������������� 0 �� 1 �δ������ӽ���(����1)�У������ȶԵ�һ����Ҫִ�еĳ���(shell)�Ļ������г�ʼ����Ȼ���Ե�¼ shell ��ʽ���ظó���ִ��֮
void init(void)
{
	int pid,i;

    // ���ڶ�ȡӲ�̲���������������Ϣ������������(�����ڵĻ�)�Ͱ�װ���ļ�ϵͳ�豸  kernel/blk_drv/hd.c 
	setup((void *) &drive_info);     // drive_info �ṹ���� 2 ��Ӳ�̲�����
    // ����ǰ��� "(void)" ǰ׺���ڱ�ʾǿ�ƺ������践��ֵ
	(void) open("/dev/tty0",O_RDWR,0);   // ��д���ʷ�ʽ���豸 "/dev/tty0",����Ӧ�ն˿���̨�������ǵ�һ�δ��ļ���������˲������ļ�����ſ϶���0���þ����ϵͳĬ�ϵĿ���̨��׼������ stdin
	(void) dup(0);                                   // ��������ľ����������� 1 ��--stdout ��׼����豸    
	(void) dup(0);                                   // ��������ľ����������� 2 ��--stderr ��׼��������豸
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS, NR_BUFFERS*BLOCK_SIZE); // ���������������ֽ�����ÿ�� 1024 �ֽ�
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);  // ס�ڴ��������ڴ��ֽ���

    // ����һ���������� sh �Ľ���
    if (!(pid=fork())) 
    {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);          // ������ļ�ʧ�ܣ����˳�(lib/_exit.c, 10)
        // ���п�ִ���ļ� sh,����ϵͳ�������λ���ļ�ϵͳ����
		execve("/bin/sh",argv_rc,envp_rc);     // �滻�� /bin/sh ����ִ��shell������Я���Ĳ����ͻ��������ֱ��� argv_rc �� envp_rc �������
		_exit(2);                                           // �� execve() ִ��ʧ�����˳� 
	}

    // ���Ǹ����� 1 ִ�е����,wait() �ȴ��ӽ���ֹͣ����ֹ������ֵӦ���ӽ��̵Ľ��̺� pid 
    // ��������� sh ִ�н�����������ִ�У�����������������˳��Ļ������ڽ��̺Ų�ͬ��˻����ѭ���ȴ�
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
    			close(0);close(1);close(2);       // �ر� stdin, stdout, stderr 
    			setsid();                                 // �½�һ���Ự�����ý������
    			(void) open("/dev/tty0",O_RDWR,0);  // ���´� /dev/tty0 ��Ϊ stdin 
    			(void) dup(0);     // ���Ƴ� stdout 
    			(void) dup(0);     // ���Ƴ� stderr 
    			_exit(execve("/bin/sh",argv,envp));
    		}
    		while (1)
    			if (pid == wait(&i))
    				break;
    		printf("\n\rchild %d died with code %04x\n\r",pid,i);
    		sync();      // ͬ��������ˢ�»�����
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}
