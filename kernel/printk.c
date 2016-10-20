/*
 *  linux/kernel/printk.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * When in kernel-mode, we cannot use printf, as fs is liable to
 * point to 'interesting' things. Make a printf with fs-saving, and
 * all is well.
 */
#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>
#include <linux/sched.h>

static char buf[1024];

extern int vsprintf(char * buf, const char * fmt, va_list args);

int printk(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=vsprintf(buf,fmt,args);
	va_end(args);
	__asm__("push %%fs\n\t"
		"push %%ds\n\t"
		"pop %%fs\n\t"
		"pushl %0\n\t"
		"pushl $buf\n\t"
		"pushl $0\n\t"
		"call tty_write\n\t"
		"addl $8,%%esp\n\t"
		"popl %0\n\t"
		"pop %%fs"
		::"r" (i):"ax","cx","dx");
	return i;
}

int print_task_struct(const struct task_struct *p)
{
    printk("state=%ld\n", p->state);
    printk("counter=%ld\n", p->counter);
    printk("priority=%ld\n", p->priority);
    printk("signal=%ld\n", p->signal);
    printk("start_code=%ld end_code=%ld end_data=%ld brk=%ld start_stack=%ld\n", p->start_code, p->end_code, p->end_data, p->brk, p->start_stack);
    //printk("pwd=%s\n", p->pwd);
    //printk("root=%s\n", p->root);

    printk("&ldt[0]=%ld &ldt[1]=%ld &ldt[2]=%ld\n", &(p->ldt[0]), &(p->ldt[1]), &(p->ldt[2]));
    printk("ldt[0]=%ld ldt[1]=%ld ldt[2]=%ld\n", p->ldt[0].a, (p->ldt[1].a)*65536+p->ldt[1].b, (p->ldt[2].a)*65536+p->ldt[2].b);
    printk("tss=%ld\n", &(p->tss));
    printk("cs=%ld ldt=%ld\n", p->tss.cs, p->tss.ldt);
}
