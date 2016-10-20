
// 移动到用户模式运行
// 该函数利用 iret 指令实现从内核模式移动到初始任务 0 中去执行
#define move_to_user_mode() \
__asm__ ( "movl %%esp,%%eax\n\t" \
	"pushl $0x17\n\t" \
	"pushl %%eax\n\t" \
	"pushfl\n\t" \
	"pushl $0x0f\n\t" \
	"pushl $1f\n\t" \
 	"iret\n" \
	"1:\tmovl $0x17,%%eax\n\t" \
	"movw %%ax,%%ds\n\t" \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")

/*
__asm__ ( 
    // 将原来的 esp 保存到 eax 中
    "movl %%esp,%%eax\n\t" \
    // 模拟压入用户态的 ss 寄存器的值， 0x17 对应的是 LDT 的数据段
	"pushl $0x17\n\t" \
    // 模拟压入用户态的 esp 寄存器
	"pushl %%eax\n\t" \
    // 模拟压入标志寄存器的值
	"pushfl\n\t" \
    // 模拟压入用户执行的cs,0x0f 对应的是 LDT 的代码段
	"pushl $0x0f\n\t" \
    // 模拟压入中断发生时的用户 eip,标号 1 是中断返回时要执行的地址，在下面
	"pushl $1f\n\t" \
    // 中断返回，这个时候会恢复现场，导致到用户态的转变
    // 任务 0 的堆栈就是内核的堆栈，当执行了 iret 之后，就移到了任务 0 中执行了，由于任务 0 描述符特权级是 3，所以堆栈上的 ss:esp 也会被弹出，因此在 iret 之后，esp 又等于 esp0,
	"iret\n" \
    // 中断返回时会从这里继续执行
	"1:\tmovl $0x17,%%eax\n\t" \
	"movw %%ax,%%ds\n\t" \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")
*/


#define sti() __asm__ ("sti"::)               // start interrupt  开中断嵌入汇编宏函数
#define cli() __asm__ ("cli"::)                // clear interrupt 关中断
#define nop() __asm__ ("nop"::)           // 空操作

#define iret() __asm__ ("iret"::)             // 中断返回

#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax\n\t" \
	"movw %0,%%dx\n\t" \
	"movl %%eax,%1\n\t" \
	"movl %%edx,%2" \
	: \
	: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	"o" (*((char *) (gate_addr))), \
	"o" (*(4+(char *) (gate_addr))), \
	"d" ((char *) (addr)),"a" (0x00080000))

#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)

#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
	*(gate_addr) = ((base) & 0xff000000) | \
		(((base) & 0x00ff0000)>>16) | \
		((limit) & 0xf0000) | \
		((dpl)<<13) | \
		(0x00408000) | \
		((type)<<8); \
	*((gate_addr)+1) = (((base) & 0x0000ffff)<<16) | \
		((limit) & 0x0ffff); }

#define _set_tssldt_desc(n,addr,type) \
__asm__ ("movw $104,%1\n\t" \
	"movw %%ax,%2\n\t" \
	"rorl $16,%%eax\n\t" \
	"movb %%al,%3\n\t" \
	"movb $" type ",%4\n\t" \
	"movb $0x00,%5\n\t" \
	"movb %%ah,%6\n\t" \
	"rorl $16,%%eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)

/*
__asm__ (
// 将 TSS 长度 104 放入描述符长度域(第0-1字节)
    "movw $104,%1\n\t" \
// 将基地址的低字节放入描述符第2-3字节
	"movw %%ax,%2\n\t" \
// 将基地址高字移入 ax 中
	"rorl $16,%%eax\n\t" \
// 将基地址高字中低字节移入描述符第 4 字节
	"movb %%al,%3\n\t" \
// 将标志类型字节移入描述符的第 5 字节
	"movb $" type ",%4\n\t" \
// 描述符的第 6 字节置 0
	"movb $0x00,%5\n\t" \
// 将基地址高字中高字节移入描述符第 7 字节
	"movb %%ah,%6\n\t" \
// eax 清零
	"rorl $16,%%eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)
*/

// n 是该描述符的指针， addr 是描述符的基地址值，任务状态描述符的类型是 0x89
#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x89")

// n 是该描述符的指针，addr 是描述符中的基地址值，局部表描述符的类型是 0x82
#define set_ldt_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x82")

