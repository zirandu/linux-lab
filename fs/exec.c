/*
 *  linux/fs/exec.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * #!-checking implemented by tytso.
 */

/*
 * Demand-loading implemented 01.12.91 - no need to read anything but
 * the header into memory. The inode of the executable is put into
 * "current->executable", and page faults do the actual loading. Clean.
 *
 * Once more I can proudly say that linux stood up to being changed: it
 * was less than 2 hours work to get demand-loading completely implemented.
 */

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <a.out.h>     // a.out 头文件，定义了 a.out 执行文件格式和一些宏

#include <linux/fs.h>
#include <linux/sched.h>     // 调度程序头文件，定义了任务结构 task_struct,任务 0 数据等
#include <linux/kernel.h>     // 内核头文件，还有一些内核常用函数的原型定义
#include <linux/mm.h>
#include <asm/segment.h>

extern int sys_exit(int exit_code);    // 程序退出系统调用
extern int sys_close(int fd);             // 文件关闭系统调用

/*
 * MAX_ARG_PAGES defines the number of pages allocated for arguments
 * and envelope for the new program. 32 should suffice, this gives
 * a maximum env+arg of 128kB !
 */
#define MAX_ARG_PAGES 32

/*
 * create_tables() parses the env- and arg-strings in new user
 * memory and creates the pointer tables from them, and puts their
 * addresses on the "stack", returning the new stack pointer value.
 */
// 在新任务栈中创建参数和环境变量指针表
// 参数: p 数据段中参数和环境信息偏移指针  argc  参数个数   envc  环境变量个数
// 返回: 栈指针值
static unsigned long * create_tables(char * p,int argc,int envc)
{
	unsigned long *argv,*envp;
	unsigned long * sp;

	sp = (unsigned long *) (0xfffffffc & (unsigned long) p);
	sp -= envc+1;
	envp = sp;
	sp -= argc+1;
	argv = sp;
	put_fs_long((unsigned long)envp,--sp);
	put_fs_long((unsigned long)argv,--sp);
	put_fs_long((unsigned long)argc,--sp);
	while (argc-->0) {
		put_fs_long((unsigned long) p,argv++);
		while (get_fs_byte(p++)) /* nothing */ ;
	}
	put_fs_long(0,argv);
	while (envc-->0) {
		put_fs_long((unsigned long) p,envp++);
		while (get_fs_byte(p++)) /* nothing */ ;
	}
	put_fs_long(0,envp);
	return sp;
}

/*
 * count() counts the number of arguments/envelopes
 */
// 计算参数个数
// 参数: argv 参数指针数组，最后一个指针项是 NULL 
// 统计参数指针数组中指针的个数，关于函数参数传递指针的指针的作用，请参加程序 kernel/sched.c 中第 151 行前的注释
static int count(char ** argv)
{
	int i=0;
	char ** tmp;

	if ((tmp = argv))
		while (get_fs_long((unsigned long *) (tmp++)))
			i++;

	return i;
}

/*
 * 'copy_string()' copies argument/envelope strings from user
 * memory to free pages in kernel mem. These are in a format ready
 * to be put directly into the top of new user memory.
 *
 * Modified by TYT, 11/24/91 to add the from_kmem argument, which specifies
 * whether the string and the string array are from user or kernel segments:
 * 
 * from_kmem     argv *        argv **
 *    0          user space    user space
 *    1          kernel space  user space
 *    2          kernel space  kernel space
 * 
 * We do this by playing games with the fs segment register.  Since it
 * it is expensive to load a segment register, we try to avoid calling
 * set_fs() unless we absolutely have to.
 */
static unsigned long copy_strings(int argc,char ** argv,unsigned long *page,
		unsigned long p, int from_kmem)
{
	char *tmp, *pag=NULL;
	int len, offset = 0;
	unsigned long old_fs, new_fs;

	if (!p)
		return 0;	/* bullet-proofing */
	new_fs = get_ds();
	old_fs = get_fs();
	if (from_kmem==2)
		set_fs(new_fs);
	while (argc-- > 0) {
		if (from_kmem == 1)
			set_fs(new_fs);
		if (!(tmp = (char *)get_fs_long(((unsigned long *)argv)+argc)))
			panic("argc is wrong");
		if (from_kmem == 1)
			set_fs(old_fs);
		len=0;		/* remember zero-padding */
		do {
			len++;
		} while (get_fs_byte(tmp++));
		if (p-len < 0) {	/* this shouldn't happen - 128kB */
			set_fs(old_fs);
			return 0;
		}
		while (len) {
			--p; --tmp; --len;
			if (--offset < 0) {
				offset = p % PAGE_SIZE;
				if (from_kmem==2)
					set_fs(old_fs);
				if (!(pag = (char *) page[p/PAGE_SIZE]) &&
				    !(pag = (char *) (page[p/PAGE_SIZE] =
				      get_free_page()))) 
					return 0;
				if (from_kmem==2)
					set_fs(new_fs);

			}
			*(pag + offset) = get_fs_byte(tmp);
		}
	}
	if (from_kmem==2)
		set_fs(old_fs);
	return p;
}

static unsigned long change_ldt(unsigned long text_size,unsigned long * page)
{
	unsigned long code_limit,data_limit,code_base,data_base;
	int i;

	code_limit = text_size+PAGE_SIZE -1;
	code_limit &= 0xFFFFF000;
	data_limit = 0x4000000;
	code_base = get_base(current->ldt[1]);
	data_base = code_base;
	set_base(current->ldt[1],code_base);
	set_limit(current->ldt[1],code_limit);
	set_base(current->ldt[2],data_base);
	set_limit(current->ldt[2],data_limit);
/* make sure fs points to the NEW data segment */
	__asm__("pushl $0x17\n\tpop %%fs"::);
	data_base += data_limit;
	for (i=MAX_ARG_PAGES-1 ; i>=0 ; i--) {
		data_base -= PAGE_SIZE;
		if (page[i])
			put_page(page[i],data_base);
	}
	return data_limit;
}

/*
 * 'do_execve()' executes a new program.
 */
// execv() 系统中断调用函数，加载并执行子进程(其他程序)
// 该函数是系统中断调用(int 0x80)功能号 __NR_execve 调用的函数，函数的参数是进入系统调用处理过程后直到调用本系统调用处理过程(system_call.s 第200行)和
// 调用本函数之前 (system_call.s 第203行)逐步压入栈中的值，这些值包括:
// 1.第 86-88 行入堆的 edx,ecx,ebx 寄存器的值，分别对应 **envp,**argc和*filename 
// 2.第94行调用 sys_call_table 中 sys_execve 函数(指针)时压入栈的函数返回地址(tmp)
// 3.第 202 行在调用本函数 do_execve 前入栈的指向栈中调用系统中断的程序代码指针 eip 
// 参数:
// eip - 调用系统中断的程序代码指针，参见 kernel/system_call.s 程序开始部分的说明
// tmp- 系统中断在调用 _sys_execve 时的返回地址，无用
// filename- 被执行程序文件名指针
// argv - 命令行参数指针数组的指针
// envp - 环境变量指针数组的指针
// 返回: 如果调用成功，则不返回，否则设置出错号，并返回 -1 
int do_execve(unsigned long * eip,long tmp,char * filename,
	char ** argv, char ** envp)
{
	struct m_inode * inode;         // 内存中 i 节点指针
	struct buffer_head * bh;        // 高速缓存块头指针
	struct exec ex;                      // 执行头文件头部数据结构变量
	unsigned long page[MAX_ARG_PAGES];     // 参数和环境串空间页面指针数组
	int i,argc,envc;         
	int e_uid, e_gid;          // 有效用户 ID 和有效组 ID
	int retval;                   // 返回值
	int sh_bang = 0;         // 控制是否需要执行脚本程序
	unsigned long p=PAGE_SIZE*MAX_ARG_PAGES-4;     // p 指向参数和环境空间的最后部  
/*
在正式设置执行文件的运行环境之前。内核准备了 128KB(32个页面，一个页面4KB)空间来存放执行文件的命令行参数和环境字符串，上行把 p 初始设置成位于 
128KB 空间的最后 1 个长字处。在初始参数和环境空间的操作过程中，p 将用来指明在 128KB空间中的当前位置 
 参数 eip[1] 是调用本次系统调用的原用户程序代码段寄存器CS值，其中的段选择符当然是必须是当前任务的代码段选择符(0x000f).若不是该值，那么CS只能会是
 内核代码段的选择符 0x0008。但这是绝对不允许的，因为内核代码是常驻内存而不能被替换掉的。因此下面根据 eip[1] 的值确认是否符合正常情况，然后再初始化
 128KB 的参数和环境串空间，把所有字节清零，并取出执行文件的 i 节点，在根据函数参数分别计算出命令行参数和环境字符串的个数 argc 和 envc
*/ 
	if ((0xffff & eip[1]) != 0x000f)
		panic("execve called from supervisor mode");
	for (i=0 ; i<MAX_ARG_PAGES ; i++)	/* clear page-table */    // 初始化 128KB的参数和环境串空间
		page[i]=0;
	if (!(inode=namei(filename)))		/* get executables inode */
		return -ENOENT;
	argc = count(argv);        // 命令行参数个数
	envc = count(envp);       // 环境字符串变量个数 
	
restart_interp:
	if (!S_ISREG(inode->i_mode)) {	/* must be regular file */   // 执行文件必须是常规文件
		retval = -EACCES;
		goto exec_error2;
	}
/*
检查当前进程是否有权运行指定的执行文件，即根据执行文件 i 节点中的属性，看看本进程是否有权执行它 
在把执行文件 i 节点的属性字段值取到 i 中后，我们首先查看属性中是否设置了 "设置-用户-ID"(set-user-id)标志和"设置-组-ID"(set-group-id)标志 
这两个标志主要让一般用户能够执行特权用户(如超级用户root)的程序，例如改变程序的 passwd 等。如果 set-user-id 标志置位，则后面执行进程的有效用户 ID(euid)就设置成执行文件 
的用户ID，否则设置成当前进程的 euid.如果执行文件 set-group-id 被置位的话，则执行进程的有效组ID(egid) 就设置为执行文件的组 ID。否则设置成当前进程的 egid,这里暂时把这 
两个判断出来的值保存在变量 e_uid 和 e_gid 中 
*/
	i = inode->i_mode;     // 取文件属性字段值 
	e_uid = (i & S_ISUID) ? inode->i_uid : current->euid;     // struct task_struct *current = &(init_task.task);  kernel/sched.c  中定义的全局变量，当前进程
	e_gid = (i & S_ISGID) ? inode->i_gid : current->egid;


	if (current->euid == inode->i_uid)
		i >>= 6;
	else if (current->egid == inode->i_gid)
		i >>= 3;
	if (!(i & 1) &&
	    !((inode->i_mode & 0111) && suser())) {
		retval = -ENOEXEC;
		goto exec_error2;
	}
/*
  
*/
	if (!(bh = bread(inode->i_dev,inode->i_zone[0]))) {   
		retval = -EACCES;
		goto exec_error2;
	}
	ex = *((struct exec *) bh->b_data);	/* read exec-header */
	if ((bh->b_data[0] == '#') && (bh->b_data[1] == '!') && (!sh_bang)) {
		/*
		 * This section does the #! interpretation.
		 * Sorta complicated, but hopefully it will work.  -TYT
		 */

		char buf[1023], *cp, *interp, *i_name, *i_arg;
		unsigned long old_fs;

		strncpy(buf, bh->b_data+2, 1022);
		brelse(bh);
		iput(inode);
		buf[1022] = '\0';
		if ((cp = strchr(buf, '\n'))) {
			*cp = '\0';
			for (cp = buf; (*cp == ' ') || (*cp == '\t'); cp++);
		}
		if (!cp || *cp == '\0') {
			retval = -ENOEXEC; /* No interpreter name found */
			goto exec_error1;
		}
		interp = i_name = cp;
		i_arg = 0;
		for ( ; *cp && (*cp != ' ') && (*cp != '\t'); cp++) {
 			if (*cp == '/')
				i_name = cp+1;
		}
		if (*cp) {
			*cp++ = '\0';
			i_arg = cp;
		}
		/*
		 * OK, we've parsed out the interpreter name and
		 * (optional) argument.
		 */
		if (sh_bang++ == 0) {
			p = copy_strings(envc, envp, page, p, 0);
			p = copy_strings(--argc, argv+1, page, p, 0);
		}
		/*
		 * Splice in (1) the interpreter's name for argv[0]
		 *           (2) (optional) argument to interpreter
		 *           (3) filename of shell script
		 *
		 * This is done in reverse order, because of how the
		 * user environment and arguments are stored.
		 */
		p = copy_strings(1, &filename, page, p, 1);
		argc++;
		if (i_arg) {
			p = copy_strings(1, &i_arg, page, p, 2);
			argc++;
		}
		p = copy_strings(1, &i_name, page, p, 2);
		argc++;
		if (!p) {
			retval = -ENOMEM;
			goto exec_error1;
		}
		/*
		 * OK, now restart the process with the interpreter's inode.
		 */
		old_fs = get_fs();
		set_fs(get_ds());
		if (!(inode=namei(interp))) { /* get executables inode */
			set_fs(old_fs);
			retval = -ENOENT;
			goto exec_error1;
		}
		set_fs(old_fs);
		goto restart_interp;
	}
	brelse(bh);
	if (N_MAGIC(ex) != ZMAGIC || ex.a_trsize || ex.a_drsize ||
		ex.a_text+ex.a_data+ex.a_bss>0x3000000 ||
		inode->i_size < ex.a_text+ex.a_data+ex.a_syms+N_TXTOFF(ex)) {
		retval = -ENOEXEC;
		goto exec_error2;
	}
	if (N_TXTOFF(ex) != BLOCK_SIZE) {
		printk("%s: N_TXTOFF != BLOCK_SIZE. See a.out.h.", filename);
		retval = -ENOEXEC;
		goto exec_error2;
	}
	if (!sh_bang) {
		p = copy_strings(envc,envp,page,p,0);
		p = copy_strings(argc,argv,page,p,0);
		if (!p) {
			retval = -ENOMEM;
			goto exec_error2;
		}
	}
/* OK, This is the point of no return */
	if (current->executable)
		iput(current->executable);
	current->executable = inode;
	for (i=0 ; i<32 ; i++) {
		if (current->sigaction[i].sa_handler != SIG_IGN)
			current->sigaction[i].sa_handler = NULL;
	}
	for (i=0 ; i<NR_OPEN ; i++)
		if ((current->close_on_exec>>i)&1)
			sys_close(i);
	current->close_on_exec = 0;
	free_page_tables(get_base(current->ldt[1]),get_limit(0x0f));
	free_page_tables(get_base(current->ldt[2]),get_limit(0x17));
	if (last_task_used_math == current)
		last_task_used_math = NULL;
	current->used_math = 0;
	p += change_ldt(ex.a_text,page)-MAX_ARG_PAGES*PAGE_SIZE;
	p = (unsigned long) create_tables((char *)p,argc,envc);
	current->brk = ex.a_bss +
		(current->end_data = ex.a_data +
		(current->end_code = ex.a_text));
	current->start_stack = p & 0xfffff000;
	current->euid = e_uid;
	current->egid = e_gid;
	i = ex.a_text+ex.a_data;
	while (i&0xfff)
		put_fs_byte(0,(char *) (i++));
	eip[0] = ex.a_entry;		/* eip, magic happens :-) */
	eip[3] = p;			/* stack pointer */
	return 0;
exec_error2:
	iput(inode);
exec_error1:
	for (i=0 ; i<MAX_ARG_PAGES ; i++)
		free_page(page[i]);
	return(retval);
}
