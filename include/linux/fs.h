/*
 * This file has definitions for some important file table
 * structures etc.
 */

#ifndef _FS_H
#define _FS_H

#include <sys/types.h>       // 类型头文件，定义了基本的数据类型

/* devices are as follows: (same as minix, so we can use the minix
 * file system. These are major numbers.)
 *
 * 0 - unused (nodev)
 * 1 - /dev/mem                   内存设备
 * 2 - /dev/fd                        软盘设备
 * 3 - /dev/hd                        硬盘设备
 * 4 - /dev/ttyx                      tty 串行终端设备
 * 5 - /dev/tty                        tty 终端设备
 * 6 - /dev/lp                         打印设备
 * 7 - unnamed pipes             没有命名的管道
 */

#define IS_SEEKABLE(x) ((x)>=1 && (x)<=3)          // 判断设备是否是可以寻找定位的  

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a))>>8)                   // 取高字节 (主设备号)
#define MINOR(a) ((a)&0xff)                                     // 取低字节 (次设备号)

#define NAME_LEN 14                    // 名字长度值
#define ROOT_INO 1                      // 根 i 节点

#define I_MAP_SLOTS 8                   // i 节点位图槽数 
#define Z_MAP_SLOTS 8                  // 逻辑块(区段块)位图槽数
#define SUPER_MAGIC 0x137F         // 文件系统魔数

#define NR_OPEN 20                         // 打开文件数，最多20个
#define NR_INODE 32                        // 系统同时最多使用 i 节点个数
#define NR_FILE 64                            // 系统最多文件个数 (文件数组项数)
#define NR_SUPER 8                          // 最多加载8种文件系统
#define NR_HASH 307                        //
#define NR_BUFFERS nr_buffers           // 数据块长度 (字节值)
#define BLOCK_SIZE 1024                   // 数据块长度所占比特数
#define BLOCK_SIZE_BITS 10
#ifndef NULL
#define NULL ((void *) 0)
#endif

#define INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct d_inode)))                     // 每个逻辑块可存放的 i 节点数
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct dir_entry)))            // 每个逻辑块可存放的目录项数

#define PIPE_HEAD(inode) ((inode).i_zone[0])                                                        // 管道头
#define PIPE_TAIL(inode) ((inode).i_zone[1])                                                          // 管道尾
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)-PIPE_TAIL(inode))&(PAGE_SIZE-1))     // 管道大小
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode)==PIPE_TAIL(inode))                       // 管道空
#define PIPE_FULL(inode) (PIPE_SIZE(inode)==(PAGE_SIZE-1))        
#define INC_PIPE(head) \                                                                                     
__asm__("incl %0\n\tandl $4095,%0"::"m" (head)) // 管道头指针递增

typedef char buffer_block[BLOCK_SIZE];         // 块缓冲区

// 缓冲块头数据结构 (极为重要)
// 在程序中常用 bh 来表示 buffer head 类型的缩写
struct buffer_head {
	char * b_data;			/* pointer to data block (1024 bytes) */
	unsigned long b_blocknr;	/* block number */     // 块号
	unsigned short b_dev;		/* device (0 = free) */   // 数据源设备号
	unsigned char b_uptodate;                                     // 更新标志: 表示数据是否已更新
	unsigned char b_dirt;		/* 0-clean,1-dirty */                 // 修改标志: 0 未修改，1已修改
	unsigned char b_count;		/* users using this block */       // 使用的用户数
	unsigned char b_lock;		/* 0 - ok, 1 -locked */              // 缓冲区是否被锁定
	struct task_struct * b_wait;               // 指向等待该缓冲区解锁的任务
	struct buffer_head * b_prev;             // hash 队列上前一块
	struct buffer_head * b_next;             // hash  队列上下一块
	struct buffer_head * b_prev_free;      // 空闲表上前一块
	struct buffer_head * b_next_free;      // 空闲表上下一块
};

// d_    为 disk     m_   为  memory 
// 磁盘上的索引节点(i 节点)数据结构
struct d_inode {
	unsigned short i_mode;       // 文件类型和属性(rwx位)
	unsigned short i_uid;           // 用户id (文件拥有者标志符)
	unsigned long i_size;           // 文件大小(字节数)
	unsigned long i_time;          // 修改时间 
	unsigned char i_gid;             // 组id(文件拥有者所在的组) 
	unsigned char i_nlinks;         // 链接数(多少个文件目录项指向该 i 节点)
	unsigned short i_zone[9];    // 直接(0-6),间接(7)或双重间接(8)逻辑块号，zone 是区的意思，可译成区段，或逻辑块
};

// d_    为 disk     m_   为  memory 
// 这是内存中的 i 节点结构，前 7 项与 d_inode 完全一样
struct m_inode {
	unsigned short i_mode;              // 文件的类型和属性   
                                                       // 0-8      RWX (宿主)  RWX (组员) RWX (其他人)     宏定义在  include/fcntl.h 
                                                       // 9-11    01:执行时设置用户ID(set-user-ID)   02:执行时设置组ID     04:对于目录，受限删除标志   
                                                       // 12-15  0001:FIFO文件(八进制)， 0002:字符设备文件， 0004:目录文件,  006:块设备文件, 010:常规文件    宏定义在 include/sys/stat.h
	unsigned short i_uid;                  // 文件宿主的用户 id
	unsigned long i_size;                  // 文件长度(字节)
	unsigned long i_mtime;               // 修改时间(从 1970.1.1 时算起，秒)
	unsigned char i_gid;                    // 文件宿主的组 id 
	unsigned char i_nlinks;                // 链接数(有多少个文件目录项指向该 i  节点)
	unsigned short i_zone[9];           // 文件所占用的盘上逻辑块号数组，其中 zone[0]-zone[6] 是直接块号，zone[7] 是一次间接块号，zone[8] 是二次(双重)间接块号
/* 上面字段在盘上和内存中的字段，共32字节*/
/* these are in memory also */
	struct task_struct * i_wait;            // 等待该 i 节点的进程
	unsigned long i_atime;               // 最后访问时间
	unsigned long i_ctime;               // i 节点自身被修改时间
	unsigned short i_dev;                // i 节点所在的设备号
	unsigned short i_num;               // i 节点号
	unsigned short i_count;              // i 节点被引用的次数，0 表示空闲
	unsigned char i_lock;                  // i 节点被锁定标志
	unsigned char i_dirt;                   // i 节点已被修改(脏)标志
	unsigned char i_pipe;                 // i 节点用作管道标志
	unsigned char i_mount;              // i 节点安装了其他文件系统标志
	unsigned char i_seek;                 // 搜索标志(lseek操作时)
	unsigned char i_update;              // i 节点已更新标志
};

// 文件结构(用于在文件句柄与 i 节点之间建立关系)
struct file {                                     // 用于在文件句柄与内存 i 节点表中 i 节点项之间建立关系
	unsigned short f_mode;             // 文件类型和访问属性 (RW位)       include/fcntl.h  
	unsigned short f_flags;               // 文件打开和控制的标志
	unsigned short f_count;              // 对应文件句柄引用计数
	struct m_inode * f_inode;             // 指向对应内存 i 节点，即现在系统中的 v 节点  
	off_t f_pos;                                  // 文件当前读写指针位置
};

// 内存中磁盘超级块结构
struct super_block {                                 // 对于PC来讲，一般以一个扇区的长度(512字节)作为块设备的数据块长度
	unsigned short s_ninodes;                  // inode numbers     节点数
	unsigned short s_nzones;                   // logic block numbers   逻辑块数
	unsigned short s_imap_blocks;           // i 节点位图所占数据块数 
	unsigned short s_zmap_blocks;          // 逻辑块位图所占用的数据块数
	unsigned short s_firstdatazone;          // 第一个数据逻辑块号
	unsigned short s_log_zone_size;         // log(数据块数/逻辑块)
	unsigned long s_max_size;                 // 最大文件长度
	unsigned short s_magic;                     // 文件系统幻数(0x137f)
/* 上面是出现在盘上和内存中的字段*/
/* These are only in memory */
	struct buffer_head * s_imap[8];             // i 节点位图在高速缓冲块指针数组，用于说明 i 节点是否被使用，同样是每一个 bit 代表一个 I 节点
	struct buffer_head * s_zmap[8];            // 逻辑块位图在高速缓冲块指针数组   最多8块缓冲区，每块缓冲区大小是 1024 字节，每 bit 表示一个盘块的占用状态，因此
                                                               // 缓冲块可代表 8192 个盘块，8个缓冲块总共可以表示 65536 个盘块，故 1.0 所能支持的最大块设备容量(长度)是 64 MB.
                                                               // 如果块盘被占用，则置为1
	unsigned short s_dev;                         // 超级块所在设备号
	struct m_inode * s_isup;                       // 被安装文件系统根目录 i 节点
	struct m_inode * s_imount;                   // 该文件系统被安装到的 i 节点
	unsigned long s_time;                         // 修改时间
	struct task_struct * s_wait;
	unsigned char s_lock;                          // 锁定标志
	unsigned char s_rd_only;                     // 只读标志
	unsigned char s_dirt;                           // 已被修改(脏)标志
};

// 磁盘上超级块结构，与上面前 8 项一致
struct d_super_block {
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
};

// 文件目录项结构 
struct dir_entry {                           // 文件目录项结构    16 字节
 	unsigned short inode;              // i 节点号     2 字节
	char name[NAME_LEN];              // 文件名     14 字节    文件系统会根据给定的文件名找到其 i 节点号，从而通过其对应 i 节点信息找到文件所在的磁盘块位置
};

extern struct m_inode inode_table[NR_INODE];           // 定义 i 节点表数组
extern struct file file_table[NR_FILE];                           // 文件表数组(64项)
extern struct super_block super_block[NR_SUPER];      // 超级块数组(8项)
extern struct buffer_head * start_buffer;                     // 缓冲区起始内存位置
extern int nr_buffers;                                                 // 缓冲块数

extern void check_disk_change(int dev);                    // 检测驱动器中软盘是否改变
extern int floppy_change(unsigned int nr);                 // 检测指定软驱动中软盘更换情况，如果软盘更换了则返回 1，否则返回 0
extern int ticks_to_floppy_on(unsigned int dev);        // 设置启动指定驱动器所需等待时间 (设置等待定时器)
extern void floppy_on(unsigned int dev);                   // 启动指定驱动器
extern void floppy_off(unsigned int dev);                  // 关闭指定软盘驱动器
                                                                                   
extern void truncate(struct m_inode * inode);              // 将 i 节点指定的文件截为 0  
extern void sync_inodes(void);                                   // 刷新 i 节点信息
extern void wait_on(struct m_inode * inode);               // 等待指定的 i 节点
extern int bmap(struct m_inode * inode,int block);              // 逻辑块(区段，磁盘块)位图操作，取数据块 block 在设备上对应的逻辑块号
extern int create_block(struct m_inode * inode,int block);    // 创建数据块 block 在设备上对应的逻辑块，并返回在设备上的逻辑块号
extern struct m_inode * namei(const char * pathname);       // 获取指定路径名的 i 节点号
extern int open_namei(const char * pathname, int flag, int mode,      // 根据路径名为打开文件操作作准备
	struct m_inode ** res_inode);
extern void iput(struct m_inode * inode);                  // 释放一个 i 节点(回写设备)
extern struct m_inode * iget(int dev,int nr);              // 从设备读取指定节点号的一个 i 节点
extern struct m_inode * get_empty_inode(void);       // 从 i 节点表(inode_table)中获取一个空闲 i 节点项
extern struct m_inode * get_pipe_inode(void);          // 获取 (申请一)管道节点，返回为 i 节点指针(如果是NULL则失败)
extern struct buffer_head * get_hash_table(int dev, int block);    // 在哈希表中查找指定的数据块，返回找到块的缓冲头指针
extern struct buffer_head * getblk(int dev, int block);                 // 从设备读取指定块 (首先会在 hash 表中查找)
extern void ll_rw_block(int rw, struct buffer_head * bh);             // 读、写数据块
extern void brelse(struct buffer_head * buf);                                // 释放指定缓冲区块 
extern struct buffer_head * bread(int dev,int block);                     // 读取指定的数据块
extern void bread_page(unsigned long addr,int dev,int b[4]);      // 读 4 块缓冲区到指定地址的内存中
extern struct buffer_head * breada(int dev,int block,...);                // 读取头一个指定的数据块，并标记后续将要读的块
extern int new_block(int dev);                               // 向设备 dev 申请一个磁盘块(区段，磁盘块),返回逻辑块号
extern void free_block(int dev, int block);               //  释放设备数据区中的逻辑块(区段，磁盘块)block,复位指定逻辑块 block 的逻辑块位图比特位
extern struct m_inode * new_inode(int dev);           // 为设备 dev 建立一个新 i 节点，返回 i 节点号
extern void free_inode(struct m_inode * inode);     // 释放一个 i 节点(删除文件时)
extern int sync_dev(int dev);            // 刷新指定设备缓冲区
extern struct super_block * get_super(int dev);     // 读取指定设备和超级块
extern int ROOT_DEV;                

extern void mount_root(void);         // 安装根文件系统

#endif
