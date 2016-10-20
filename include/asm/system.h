
// �ƶ����û�ģʽ����
// �ú������� iret ָ��ʵ�ִ��ں�ģʽ�ƶ�����ʼ���� 0 ��ȥִ��
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
    // ��ԭ���� esp ���浽 eax ��
    "movl %%esp,%%eax\n\t" \
    // ģ��ѹ���û�̬�� ss �Ĵ�����ֵ�� 0x17 ��Ӧ���� LDT �����ݶ�
	"pushl $0x17\n\t" \
    // ģ��ѹ���û�̬�� esp �Ĵ���
	"pushl %%eax\n\t" \
    // ģ��ѹ���־�Ĵ�����ֵ
	"pushfl\n\t" \
    // ģ��ѹ���û�ִ�е�cs,0x0f ��Ӧ���� LDT �Ĵ����
	"pushl $0x0f\n\t" \
    // ģ��ѹ���жϷ���ʱ���û� eip,��� 1 ���жϷ���ʱҪִ�еĵ�ַ��������
	"pushl $1f\n\t" \
    // �жϷ��أ����ʱ���ָ��ֳ������µ��û�̬��ת��
    // ���� 0 �Ķ�ջ�����ں˵Ķ�ջ����ִ���� iret ֮�󣬾��Ƶ������� 0 ��ִ���ˣ��������� 0 ��������Ȩ���� 3�����Զ�ջ�ϵ� ss:esp Ҳ�ᱻ����������� iret ֮��esp �ֵ��� esp0,
	"iret\n" \
    // �жϷ���ʱ����������ִ��
	"1:\tmovl $0x17,%%eax\n\t" \
	"movw %%ax,%%ds\n\t" \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")
*/


#define sti() __asm__ ("sti"::)               // start interrupt  ���ж�Ƕ����꺯��
#define cli() __asm__ ("cli"::)                // clear interrupt ���ж�
#define nop() __asm__ ("nop"::)           // �ղ���

#define iret() __asm__ ("iret"::)             // �жϷ���

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
// �� TSS ���� 104 ����������������(��0-1�ֽ�)
    "movw $104,%1\n\t" \
// ������ַ�ĵ��ֽڷ�����������2-3�ֽ�
	"movw %%ax,%2\n\t" \
// ������ַ�������� ax ��
	"rorl $16,%%eax\n\t" \
// ������ַ�����е��ֽ������������� 4 �ֽ�
	"movb %%al,%3\n\t" \
// ����־�����ֽ������������ĵ� 5 �ֽ�
	"movb $" type ",%4\n\t" \
// �������ĵ� 6 �ֽ��� 0
	"movb $0x00,%5\n\t" \
// ������ַ�����и��ֽ������������� 7 �ֽ�
	"movb %%ah,%6\n\t" \
// eax ����
	"rorl $16,%%eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)
*/

// n �Ǹ���������ָ�룬 addr ���������Ļ���ֵַ������״̬�������������� 0x89
#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x89")

// n �Ǹ���������ָ�룬addr ���������еĻ���ֵַ���ֲ����������������� 0x82
#define set_ldt_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x82")

