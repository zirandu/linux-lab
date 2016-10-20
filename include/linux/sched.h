#ifndef _SCHED_H     // schedule ���Լ�дΪ sched.h 
#define _SCHED_H

#define NR_TASKS 64                     // ϵͳ��ͬʱ�������(����)��
#define HZ 100                              // ����ϵͳʱ�ӵδ�Ƶ��(1�ٺ��ȣ�ÿ���δ�10ms)

#define FIRST_TASK task[0]                     // ����0�Ƚ����⣬�������������������һ������
#define LAST_TASK task[NR_TASKS-1]     // ���������е����һ������

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <signal.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

// ���ﶨ���˽������п��ܴ���״̬
#define TASK_RUNNING		     0            // �����������л���׼������
#define TASK_INTERRUPTIBLE	  1            // ���̴��ڿ��жϵȴ�״̬
#define TASK_UNINTERRUPTIBLE	2            // ���̴��ڲ����жϵȴ�״̬����Ҫ���� I/O �����ȴ�
#define TASK_ZOMBIE		       3            // ���̴��ڽ���״̬���Ѿ�ֹͣ���У��������̻�û���ź�
#define TASK_STOPPED		      4           // ������ֹͣ

#ifndef NULL
#define NULL ((void *) 0)                       // ����NULLΪ��ָ��
#endif

// ���ƽ��̵�ҳĿ¼ҳ��Linus ��Ϊ�����ں�����ӵĺ���֮һ (mm/memory.c 105)
extern int copy_page_tables(unsigned long from, unsigned long to, long size);
// �ͷ�ҳ����ָ�����ڴ�鼰ҳ���� (mm/memory.c 150)
extern int free_page_tables(unsigned long from, unsigned long size);

extern void sched_init(void);    // ���ȳ���ĳ�ʼ������  (kernel/sched.c 385)
extern void schedule(void);      // ���̵��Ⱥ���  (kernel/sched, 104)
extern void trap_init(void);       // �쳣(����)�жϴ����ʼ�������������жϵ����������ж������ź� (kernel/traps.c 181)
#ifndef PANIC
void panic(const char * str);       // ��ʾ�ں˳�����Ϣ��Ȼ�������ѭ�� (kernel/panic.c 16)
#endif
extern int tty_write(unsigned minor,char * buf,int count);    // �� tty ��дָ�����ȵ��ַ��� (kernel/chr_drv/tty_io.c 290)

typedef int (*fn_ptr)();      // ���庯��ָ������

// ��������ѧЭ������ʹ�õĽṹ����Ҫ���ڱ�������л�ʱ i387 ��ִ��״̬��Ϣ
struct i387_struct {        
	long	cwd;             // ������ Control word  
	long	swd;             // ״̬�� Status word 
	long	twd;              // ����� Tag word 
	long	fip;               // Э����������ָ��
	long	fcs;               // Э����������μĴ���
	long	foo;              // �ڴ��������ƫ��λ��
	long	fos;               // �ڴ�������Ķ�ֵ
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */  // 8��10�ֽڵ�Э�����ۼ���
};

// ����״̬�����ݽṹ    p332  ͼ 8-14
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

// ����������
struct task_struct {
/* these are hardcoded - don't touch */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */             // ���������״̬ (-1�������У�0������(����)��>0��ֹͣ)
	long counter;                                                                            // ��������ʱ�����(�ݼ�)(�δ���)������ʱ��Ƭ
	long priority;                                                                             // ����������������ʼ����ʱ counter = priority,Խ������Խ��
	long signal;                                                                              // �źţ���λͼ��ÿ������λ����һ���źţ��ź�ֵ=λƫ��ֵ+1 
	struct sigaction sigaction[32];                                                     // �ź�ִ�����Խṹ����Ӧ�źŽ�Ҫִ�еĲ����ͱ�־��Ϣ
	long blocked;	/* bitmap of masked signals */                            // �����ź�������(��Ӧ�ź�λͼ)
/* various fields */
	int exit_code;                                                                             // ����ִ��ֹͣ���˳��룬�丸���̻�ȡ
	unsigned long start_code,end_code,end_data,brk,start_stack;      // ����ε�ַ�����볤��(�ֽ���)�����볤��+���ݳ���(�ֽ���)���ܳ���(�ֽ���)����ջ�ε�ַ
	long pid,father,pgrp,session,leader;                                            // ���̱�ʶ�ţ������̺ţ�������ţ��Ự�ţ��Ự����
	unsigned short uid,euid,suid;                                                     // �û���ʶ��(�û�id),��Ч�û�id,������û�id 
	unsigned short gid,egid,sgid;                                                     // ���ʶ��(��id),��Ч��id,�������id 
	long alarm;                                                                                // ������ʱֵ
	long utime,stime,cutime,cstime,start_time;                                    // �û�̬����ʱ�� (�δ���),ϵͳ̬����ʱ��(�δ���),�ӽ����û�̬����ʱ��(�δ���),�ӽ���ϵͳ̬����ʱ�䣬���̿�ʼ����ʱ��
	unsigned short used_math;                                                        // ��־:�Ƿ�ʹ����Э������
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */                         // ����ʹ�� tty �����豸�ţ� -1 ��ʾû��ʹ��
	unsigned short umask;                                                              // �ļ�������������λ
	struct m_inode * pwd;                                                                 // ��ǰ����Ŀ¼ i �ڵ�ṹ,���ڽ������·����
	struct m_inode * root;                                                                  // ��Ŀ¼ i �ڵ�ṹ�����ڽ�������·����
	struct m_inode * executable;                                                        // ִ���ļ� i �ڵ�ṹ
	unsigned long close_on_exec;                                                      // ִ��ʱ�ر��ļ����λͼ��־   include/fcntl.h
	struct file * filp[NR_OPEN];                                                            // ����ʹ�õ��ļ���ṹ
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];                                                               // ldt ������ľֲ���������  0-�գ� 1-����� cs, 2-���ݺͶ�ջ�� ds&ss
/* tss for this task */
	struct tss_struct tss;                                                                      // tss �����̵�����״̬����Ϣ�ṹ 
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 */
// state=0(TASK_RUNNING)��ʾ��������(����)
// counter=15 �������е�ʱ��Ƭ(150ms),��ʼ��ʱ���� priority ��ֵ���
// priority=15 ���е�����Ȩ
// signal=0 ��ʾû�������ź�
// sigaction[32]={{},} �ź�������Ϊ�� 
// blocked=0 ��ʾ�������κ��ź�
// exit_code=0 
// start_code=0 �������ʼ��ַ
// end_code=0  ����εĳ���
// end_data=0  ���ݶεĳ���
// brk=0
// start_stack=0 ��ʾ��û�з���ջ
// pid=0  0�Ž���
// father=-1 �����̺�(��ʾû�и�����)
// pgrp=0 ���������
// session=0  
// leader=0
// uid=0     �û���ʶ��(�û�id)
// euid=0   ��Ч�û�id
// suid=0    ������û�id
// gid=0     ���ʶ��(��id)
// egid=0   ��Ч��id
// sgid=0   �������id
// alarm=0         ������ʱֵ(û���趨)
// utime=0         �û�̬ʱ��
// stime=0         ϵͳ̬ʱ��
// cutime=0       �ӽ����û�̬ʱ��
// cstime=0       �ӽ���ϵͳ̬����ʱ��
// start_time=0  ���̿�ʼ����ʱ��
// used_math=0  �Ƿ�ʹ����Э������ 
// tty=-1                   ����û��ʹ��tty
// umask=0022         �ļ�������������λ
// pwd=NULL             ��ǰ����Ŀ¼i�ڵ�
// root=NULL              ��ǰ����Ŀ¼i�ڵ�
// executable=NULL   ִ���ļ�i�ڵ�
// close_on_exec=0    ִ��ʱ�ر��ļ����λͼ��־
// filp[20]={NULL}      ����ʹ�õ��ļ���ṹ
// ldt[3]
// ���볤 640K, ��ַ0x0, G=1, D=1, DPL=3, P=1, TYPE=0x0a 
// ���ݳ� 640K, ��ַ0x0, G=1, D=1, DPL=3, P=1, TYPE=0x02 
// tss 

// ��ʼ��������һ�н��̵����Ƚ��̣���ͨ��"Ӳ����"д�������еģ���ʼ���̴�������ں˵Ľ��̣����������ᱻ���ȳ������
// ����Ŀ�ĵ�����Ϊ�����ں����У�����Ϊ�����Ľ����ṩһ�����ƵĻ��㣬��task_struct�ṹ��ֱ���ô���д�ɵĵģ���Ϊ�����ռ��ʱ��ֵ���趨����
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

extern struct task_struct *task[NR_TASKS];              // ����ָ������  NR_TASKS=64          
extern struct task_struct *last_task_used_math;       // ��һ��ʹ�ù�Э�������Ľ���
extern struct task_struct *current;                          // ��ǰ���̽ṹָ�����
extern long volatile jiffies;                                      // �ӿ�����ʼ����ĵδ���(10ms/�δ�)
extern long startup_time;                                      // ����ʱ�䣬�� 1970:0:0:0 ��ʼ����ʱ������

#define CURRENT_TIME (startup_time+jiffies/HZ)    // ��ǰʱ��(����)

extern void add_timer(long jiffies, void (*fn)(void));                  // ��Ӷ�ʱ������ (��ʱʱ�� jiffies �δ�������ʱ��ʱ���ú���*fn())   kernel/sched.c 
extern void sleep_on(struct task_struct ** p);                            // �����жϵĵȴ�˯�� kernel/sched.c 151 
extern void interruptible_sleep_on(struct task_struct ** p);       // ���жϵĵȴ�˯�� kernel/sched.c 167 
extern void wake_up(struct task_struct ** p);                            // ��ȷ����˯�ߵĽ���  

/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
/*
�� GDT ����Ѱ�ҵ�һ��TSS����ڣ�0-û����nul,1-�����cs,2-���ݶ�ds,3-ϵͳ����syscall,4-����״̬��TSS0,5-�ֲ���LTD0,6-����״̬TSS1 �ȵ� 
*/
// ��Ӣ��ע�Ϳ��Բ��뵽��Linus��ʱ���ϵͳ���õĴ���ר�ŷ��� GDT ���е� 4 �������Ķ��У���������û�������������Ǿ�һֱ�� GDT ���е� 4 ����������  
// ���� syscall �� ������һ��

/*
0     |         NULL      |
1     | ϵͳ����� CS |
2     | ϵͳ���ݶ� DS |
3     |    ϵͳ����      |
4     |        TSS0       |         <-- FIRST_TSS_ENTRY    4
5     |        LDT0      |         <-- FIRST_LDT_ENTRY    5
6     |        TSS1       |
7     |        LDT1      |
       |                      |
       |                      |
254 |      TSS126     |
255 |      LDT126    |

һ����8�ֽ� 8*8=64λ 256*8 = 2048 = 2KB

*/

#define FIRST_TSS_ENTRY 4                                                                   // ȫ�ֱ��е�һ������״̬�� TSS ��������ѡ��������      4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)                                    // ȫ�ֱ��е�һ���ֲ���������(LDT)��������ѡ��������   5
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))         // ���̺�ͬ��Ӧ�� TSS ת����ʽ: FIRST_TSS_ENTRY*8�ֽ�+n*16�ֽ�
// n=0:  4*8+0*16= 32
// n=1:  4*8+1*16= 32+16=38
// ������ȫ�ֱ��е� n ������� TSS ����������ѡ���ֵ(ƫ����) 
// ��ÿ��������ռ8�ֽڣ���� FIRST_TSS_ENTRY<<3 ( FIRST_TSS_ENTRY * 8 )��ʾ���������� GDT ���е���ʼƫ��λ��
// ��Ϊÿ������ʹ�� 1 �� TSS �� 1 �� LDT ����������ռ�� 16 �ֽڣ������Ҫ n <<4 ( * 16 ) ����ʾ��Ӧ TSS ��ʼλ��

#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))       // ���̺�ͬ��Ӧ�� LDT ת����ʽ: FIRST_LDT_ENTRY*8�ֽ�+n*16�ֽ� 
#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))                                        // load tss register                �ѵ� n �Ž��̵� TSS ��ѡ������ص�����Ĵ��� TR ��
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))                                    // load local descriptor table �ѵ� n �Ž��̵� LDT ��ѡ������ص��ֲ���������Ĵ��� LDTR �� 

// ȡ��ǰ�������������� (�����������е�����ֵ������̺� pid ��ͬ)
// ����: n - ��ǰ����ţ����� kernel/traps.c   

// ������Ĵ����� TSS �ε�ѡ������Ƶ� ax ��
// (eax - FIRST_TSS_ENTRY*8) --> eax 
// (eax/16)-->eax  = ��ǰ�����
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


#define PAGE_ALIGN(n) (((n)+0xfff)&0xfffff000)      // ҳ���ַ��׼(���ں˴�����û���κεط�����)

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

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , (base) )              // ���þֲ����������� ldt �������Ļ���ַ�ֶ�
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )   // ���þֲ����������� ldt �������Ķγ��ֶ�

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

// �ӵ�ַ addr ����������ȡ�λ���ַ�������� _set_base() �����෴
// edx - ��Ż���ַ(__base); %1 - ��ַ addr ƫ�� 2; %2 - ��ַ addr ƫ��4; %3 - addr ƫ�� 7 
static inline unsigned long _get_base(char * addr)
{
         unsigned long __base;
         __asm__("movb %3,%%dh\n\t"         // ȡ [addr+7]����ַ��16λ�ĸ�8λ(λ31-24)-->dh 
                 "movb %2,%%dl\n\t"                // ȡ [addr+4]����ַ��16λ�ĵ�8λ(λ23-16)-->dl
                 "shll $16,%%edx\n\t"              // ����ַ��16λ�Ƶ� edx �и� 16 λ��
                 "movw %1,%%dx"                    // ȡ [addr+2] ������ַ�� 16 λ(λ 15-0)-->dx 
                 :"=&d" (__base)                     // �Ӷ� edx �к��� 32 λ�Ķλ���ַ 
                 :"m" (*((addr)+2)),
                  "m" (*((addr)+4)),
                  "m" (*((addr)+7)));
         return __base;
}

#define get_base(ldt) _get_base( ((char *)&(ldt)) )       // ȡ�ֲ����������� ldt ��ָ���������Ļ���ַ

// ȡ��ѡ��� segment ָ�����������еĶ��޳�ֵ 
// ָ�� lsl �� Load Segment Limit ��д������ָ������������ȡ����ɢ���޳�����λƴ�������Ķ��޳�ֵ����ָ���Ĵ����У����õĶ��޳���ʵ���ֽ�����1
// ������ﻹ��Ҫ�� 1 ����ܷ���
// %0 - ��Ŷγ�ֵ(�ֽ���): %1 - ��ѡ��� segment 

#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

#endif
