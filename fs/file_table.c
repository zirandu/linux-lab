/*
 *  linux/fs/file_table.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <linux/fs.h>

struct file file_table[NR_FILE];   // 文件表数组，共64项 ，因此整个系统同时最多打开64个文件
//在进程的数据结构(即进程控制块或称进程描述符)中，专门定义有本进程打开文件的文件结构指针数组 filp[NR_OPEN]字段
//NR_OPEN=20,因此每个进程最多可同时打开20个文件。
//该指针数组项的顺序号即对应文件的描述符值，而项的指针则指向文件表中打开的文件项，例如，filp[0] 即是进程当前打开文件描述符 0 对应的文件结构指针。
