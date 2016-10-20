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
#include <a.out.h>     // a.out ͷ�ļ��������� a.out ִ���ļ���ʽ��һЩ��

#include <linux/fs.h>
#include <linux/sched.h>     // ���ȳ���ͷ�ļ�������������ṹ task_struct,���� 0 ���ݵ�
#include <linux/kernel.h>     // �ں�ͷ�ļ�������һЩ�ں˳��ú�����ԭ�Ͷ���
#include <linux/mm.h>
#include <asm/segment.h>

extern int sys_exit(int exit_code);    // �����˳�ϵͳ����
extern int sys_close(int fd);             // �ļ��ر�ϵͳ����

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
// ��������ջ�д��������ͻ�������ָ���
// ����: p ���ݶ��в����ͻ�����Ϣƫ��ָ��  argc  ��������   envc  ������������
// ����: ջָ��ֵ
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
// �����������
// ����: argv ����ָ�����飬���һ��ָ������ NULL 
// ͳ�Ʋ���ָ��������ָ��ĸ��������ں�����������ָ���ָ������ã���μӳ��� kernel/sched.c �е� 151 ��ǰ��ע��
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
// execv() ϵͳ�жϵ��ú��������ز�ִ���ӽ���(��������)
// �ú�����ϵͳ�жϵ���(int 0x80)���ܺ� __NR_execve ���õĺ����������Ĳ����ǽ���ϵͳ���ô�����̺�ֱ�����ñ�ϵͳ���ô������(system_call.s ��200��)��
// ���ñ�����֮ǰ (system_call.s ��203��)��ѹ��ջ�е�ֵ����Щֵ����:
// 1.�� 86-88 ����ѵ� edx,ecx,ebx �Ĵ�����ֵ���ֱ��Ӧ **envp,**argc��*filename 
// 2.��94�е��� sys_call_table �� sys_execve ����(ָ��)ʱѹ��ջ�ĺ������ص�ַ(tmp)
// 3.�� 202 ���ڵ��ñ����� do_execve ǰ��ջ��ָ��ջ�е���ϵͳ�жϵĳ������ָ�� eip 
// ����:
// eip - ����ϵͳ�жϵĳ������ָ�룬�μ� kernel/system_call.s ����ʼ���ֵ�˵��
// tmp- ϵͳ�ж��ڵ��� _sys_execve ʱ�ķ��ص�ַ������
// filename- ��ִ�г����ļ���ָ��
// argv - �����в���ָ�������ָ��
// envp - ��������ָ�������ָ��
// ����: ������óɹ����򲻷��أ��������ó���ţ������� -1 
int do_execve(unsigned long * eip,long tmp,char * filename,
	char ** argv, char ** envp)
{
	struct m_inode * inode;         // �ڴ��� i �ڵ�ָ��
	struct buffer_head * bh;        // ���ٻ����ͷָ��
	struct exec ex;                      // ִ��ͷ�ļ�ͷ�����ݽṹ����
	unsigned long page[MAX_ARG_PAGES];     // �����ͻ������ռ�ҳ��ָ������
	int i,argc,envc;         
	int e_uid, e_gid;          // ��Ч�û� ID ����Ч�� ID
	int retval;                   // ����ֵ
	int sh_bang = 0;         // �����Ƿ���Ҫִ�нű�����
	unsigned long p=PAGE_SIZE*MAX_ARG_PAGES-4;     // p ָ������ͻ����ռ�����  
/*
����ʽ����ִ���ļ������л���֮ǰ���ں�׼���� 128KB(32��ҳ�棬һ��ҳ��4KB)�ռ������ִ���ļ��������в����ͻ����ַ��������а� p ��ʼ���ó�λ�� 
128KB �ռ����� 1 �����ִ����ڳ�ʼ�����ͻ����ռ�Ĳ��������У�p ������ָ���� 128KB�ռ��еĵ�ǰλ�� 
 ���� eip[1] �ǵ��ñ���ϵͳ���õ�ԭ�û��������μĴ���CSֵ�����еĶ�ѡ�����Ȼ�Ǳ����ǵ�ǰ����Ĵ����ѡ���(0x000f).�����Ǹ�ֵ����ôCSֻ�ܻ���
 �ں˴���ε�ѡ��� 0x0008�������Ǿ��Բ�����ģ���Ϊ�ں˴����ǳ�פ�ڴ�����ܱ��滻���ġ����������� eip[1] ��ֵȷ���Ƿ�������������Ȼ���ٳ�ʼ��
 128KB �Ĳ����ͻ������ռ䣬�������ֽ����㣬��ȡ��ִ���ļ��� i �ڵ㣬�ڸ��ݺ��������ֱ����������в����ͻ����ַ����ĸ��� argc �� envc
*/ 
	if ((0xffff & eip[1]) != 0x000f)
		panic("execve called from supervisor mode");
	for (i=0 ; i<MAX_ARG_PAGES ; i++)	/* clear page-table */    // ��ʼ�� 128KB�Ĳ����ͻ������ռ�
		page[i]=0;
	if (!(inode=namei(filename)))		/* get executables inode */
		return -ENOENT;
	argc = count(argv);        // �����в�������
	envc = count(envp);       // �����ַ����������� 
	
restart_interp:
	if (!S_ISREG(inode->i_mode)) {	/* must be regular file */   // ִ���ļ������ǳ����ļ�
		retval = -EACCES;
		goto exec_error2;
	}
/*
��鵱ǰ�����Ƿ���Ȩ����ָ����ִ���ļ���������ִ���ļ� i �ڵ��е����ԣ������������Ƿ���Ȩִ���� 
�ڰ�ִ���ļ� i �ڵ�������ֶ�ֵȡ�� i �к��������Ȳ鿴�������Ƿ������� "����-�û�-ID"(set-user-id)��־��"����-��-ID"(set-group-id)��־ 
��������־��Ҫ��һ���û��ܹ�ִ����Ȩ�û�(�糬���û�root)�ĳ�������ı����� passwd �ȡ���� set-user-id ��־��λ�������ִ�н��̵���Ч�û� ID(euid)�����ó�ִ���ļ� 
���û�ID���������óɵ�ǰ���̵� euid.���ִ���ļ� set-group-id ����λ�Ļ�����ִ�н��̵���Ч��ID(egid) ������Ϊִ���ļ����� ID���������óɵ�ǰ���̵� egid,������ʱ���� 
�����жϳ�����ֵ�����ڱ��� e_uid �� e_gid �� 
*/
	i = inode->i_mode;     // ȡ�ļ������ֶ�ֵ 
	e_uid = (i & S_ISUID) ? inode->i_uid : current->euid;     // struct task_struct *current = &(init_task.task);  kernel/sched.c  �ж����ȫ�ֱ�������ǰ����
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
