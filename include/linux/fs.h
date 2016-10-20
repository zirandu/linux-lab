/*
 * This file has definitions for some important file table
 * structures etc.
 */

#ifndef _FS_H
#define _FS_H

#include <sys/types.h>       // ����ͷ�ļ��������˻�������������

/* devices are as follows: (same as minix, so we can use the minix
 * file system. These are major numbers.)
 *
 * 0 - unused (nodev)
 * 1 - /dev/mem                   �ڴ��豸
 * 2 - /dev/fd                        �����豸
 * 3 - /dev/hd                        Ӳ���豸
 * 4 - /dev/ttyx                      tty �����ն��豸
 * 5 - /dev/tty                        tty �ն��豸
 * 6 - /dev/lp                         ��ӡ�豸
 * 7 - unnamed pipes             û�������Ĺܵ�
 */

#define IS_SEEKABLE(x) ((x)>=1 && (x)<=3)          // �ж��豸�Ƿ��ǿ���Ѱ�Ҷ�λ��  

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a))>>8)                   // ȡ���ֽ� (���豸��)
#define MINOR(a) ((a)&0xff)                                     // ȡ���ֽ� (���豸��)

#define NAME_LEN 14                    // ���ֳ���ֵ
#define ROOT_INO 1                      // �� i �ڵ�

#define I_MAP_SLOTS 8                   // i �ڵ�λͼ���� 
#define Z_MAP_SLOTS 8                  // �߼���(���ο�)λͼ����
#define SUPER_MAGIC 0x137F         // �ļ�ϵͳħ��

#define NR_OPEN 20                         // ���ļ��������20��
#define NR_INODE 32                        // ϵͳͬʱ���ʹ�� i �ڵ����
#define NR_FILE 64                            // ϵͳ����ļ����� (�ļ���������)
#define NR_SUPER 8                          // ������8���ļ�ϵͳ
#define NR_HASH 307                        //
#define NR_BUFFERS nr_buffers           // ���ݿ鳤�� (�ֽ�ֵ)
#define BLOCK_SIZE 1024                   // ���ݿ鳤����ռ������
#define BLOCK_SIZE_BITS 10
#ifndef NULL
#define NULL ((void *) 0)
#endif

#define INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct d_inode)))                     // ÿ���߼���ɴ�ŵ� i �ڵ���
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct dir_entry)))            // ÿ���߼���ɴ�ŵ�Ŀ¼����

#define PIPE_HEAD(inode) ((inode).i_zone[0])                                                        // �ܵ�ͷ
#define PIPE_TAIL(inode) ((inode).i_zone[1])                                                          // �ܵ�β
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)-PIPE_TAIL(inode))&(PAGE_SIZE-1))     // �ܵ���С
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode)==PIPE_TAIL(inode))                       // �ܵ���
#define PIPE_FULL(inode) (PIPE_SIZE(inode)==(PAGE_SIZE-1))        
#define INC_PIPE(head) \                                                                                     
__asm__("incl %0\n\tandl $4095,%0"::"m" (head)) // �ܵ�ͷָ�����

typedef char buffer_block[BLOCK_SIZE];         // �黺����

// �����ͷ���ݽṹ (��Ϊ��Ҫ)
// �ڳ����г��� bh ����ʾ buffer head ���͵���д
struct buffer_head {
	char * b_data;			/* pointer to data block (1024 bytes) */
	unsigned long b_blocknr;	/* block number */     // ���
	unsigned short b_dev;		/* device (0 = free) */   // ����Դ�豸��
	unsigned char b_uptodate;                                     // ���±�־: ��ʾ�����Ƿ��Ѹ���
	unsigned char b_dirt;		/* 0-clean,1-dirty */                 // �޸ı�־: 0 δ�޸ģ�1���޸�
	unsigned char b_count;		/* users using this block */       // ʹ�õ��û���
	unsigned char b_lock;		/* 0 - ok, 1 -locked */              // �������Ƿ�����
	struct task_struct * b_wait;               // ָ��ȴ��û���������������
	struct buffer_head * b_prev;             // hash ������ǰһ��
	struct buffer_head * b_next;             // hash  ��������һ��
	struct buffer_head * b_prev_free;      // ���б���ǰһ��
	struct buffer_head * b_next_free;      // ���б�����һ��
};

// d_    Ϊ disk     m_   Ϊ  memory 
// �����ϵ������ڵ�(i �ڵ�)���ݽṹ
struct d_inode {
	unsigned short i_mode;       // �ļ����ͺ�����(rwxλ)
	unsigned short i_uid;           // �û�id (�ļ�ӵ���߱�־��)
	unsigned long i_size;           // �ļ���С(�ֽ���)
	unsigned long i_time;          // �޸�ʱ�� 
	unsigned char i_gid;             // ��id(�ļ�ӵ�������ڵ���) 
	unsigned char i_nlinks;         // ������(���ٸ��ļ�Ŀ¼��ָ��� i �ڵ�)
	unsigned short i_zone[9];    // ֱ��(0-6),���(7)��˫�ؼ��(8)�߼���ţ�zone ��������˼����������Σ����߼���
};

// d_    Ϊ disk     m_   Ϊ  memory 
// �����ڴ��е� i �ڵ�ṹ��ǰ 7 ���� d_inode ��ȫһ��
struct m_inode {
	unsigned short i_mode;              // �ļ������ͺ�����   
                                                       // 0-8      RWX (����)  RWX (��Ա) RWX (������)     �궨����  include/fcntl.h 
                                                       // 9-11    01:ִ��ʱ�����û�ID(set-user-ID)   02:ִ��ʱ������ID     04:����Ŀ¼������ɾ����־   
                                                       // 12-15  0001:FIFO�ļ�(�˽���)�� 0002:�ַ��豸�ļ��� 0004:Ŀ¼�ļ�,  006:���豸�ļ�, 010:�����ļ�    �궨���� include/sys/stat.h
	unsigned short i_uid;                  // �ļ��������û� id
	unsigned long i_size;                  // �ļ�����(�ֽ�)
	unsigned long i_mtime;               // �޸�ʱ��(�� 1970.1.1 ʱ������)
	unsigned char i_gid;                    // �ļ��������� id 
	unsigned char i_nlinks;                // ������(�ж��ٸ��ļ�Ŀ¼��ָ��� i  �ڵ�)
	unsigned short i_zone[9];           // �ļ���ռ�õ������߼�������飬���� zone[0]-zone[6] ��ֱ�ӿ�ţ�zone[7] ��һ�μ�ӿ�ţ�zone[8] �Ƕ���(˫��)��ӿ��
/* �����ֶ������Ϻ��ڴ��е��ֶΣ���32�ֽ�*/
/* these are in memory also */
	struct task_struct * i_wait;            // �ȴ��� i �ڵ�Ľ���
	unsigned long i_atime;               // ������ʱ��
	unsigned long i_ctime;               // i �ڵ������޸�ʱ��
	unsigned short i_dev;                // i �ڵ����ڵ��豸��
	unsigned short i_num;               // i �ڵ��
	unsigned short i_count;              // i �ڵ㱻���õĴ�����0 ��ʾ����
	unsigned char i_lock;                  // i �ڵ㱻������־
	unsigned char i_dirt;                   // i �ڵ��ѱ��޸�(��)��־
	unsigned char i_pipe;                 // i �ڵ������ܵ���־
	unsigned char i_mount;              // i �ڵ㰲װ�������ļ�ϵͳ��־
	unsigned char i_seek;                 // ������־(lseek����ʱ)
	unsigned char i_update;              // i �ڵ��Ѹ��±�־
};

// �ļ��ṹ(�������ļ������ i �ڵ�֮�佨����ϵ)
struct file {                                     // �������ļ�������ڴ� i �ڵ���� i �ڵ���֮�佨����ϵ
	unsigned short f_mode;             // �ļ����ͺͷ������� (RWλ)       include/fcntl.h  
	unsigned short f_flags;               // �ļ��򿪺Ϳ��Ƶı�־
	unsigned short f_count;              // ��Ӧ�ļ�������ü���
	struct m_inode * f_inode;             // ָ���Ӧ�ڴ� i �ڵ㣬������ϵͳ�е� v �ڵ�  
	off_t f_pos;                                  // �ļ���ǰ��дָ��λ��
};

// �ڴ��д��̳�����ṹ
struct super_block {                                 // ����PC������һ����һ�������ĳ���(512�ֽ�)��Ϊ���豸�����ݿ鳤��
	unsigned short s_ninodes;                  // inode numbers     �ڵ���
	unsigned short s_nzones;                   // logic block numbers   �߼�����
	unsigned short s_imap_blocks;           // i �ڵ�λͼ��ռ���ݿ��� 
	unsigned short s_zmap_blocks;          // �߼���λͼ��ռ�õ����ݿ���
	unsigned short s_firstdatazone;          // ��һ�������߼����
	unsigned short s_log_zone_size;         // log(���ݿ���/�߼���)
	unsigned long s_max_size;                 // ����ļ�����
	unsigned short s_magic;                     // �ļ�ϵͳ����(0x137f)
/* �����ǳ��������Ϻ��ڴ��е��ֶ�*/
/* These are only in memory */
	struct buffer_head * s_imap[8];             // i �ڵ�λͼ�ڸ��ٻ����ָ�����飬����˵�� i �ڵ��Ƿ�ʹ�ã�ͬ����ÿһ�� bit ����һ�� I �ڵ�
	struct buffer_head * s_zmap[8];            // �߼���λͼ�ڸ��ٻ����ָ������   ���8�黺������ÿ�黺������С�� 1024 �ֽڣ�ÿ bit ��ʾһ���̿��ռ��״̬�����
                                                               // �����ɴ��� 8192 ���̿飬8��������ܹ����Ա�ʾ 65536 ���̿飬�� 1.0 ����֧�ֵ������豸����(����)�� 64 MB.
                                                               // ������̱�ռ�ã�����Ϊ1
	unsigned short s_dev;                         // �����������豸��
	struct m_inode * s_isup;                       // ����װ�ļ�ϵͳ��Ŀ¼ i �ڵ�
	struct m_inode * s_imount;                   // ���ļ�ϵͳ����װ���� i �ڵ�
	unsigned long s_time;                         // �޸�ʱ��
	struct task_struct * s_wait;
	unsigned char s_lock;                          // ������־
	unsigned char s_rd_only;                     // ֻ����־
	unsigned char s_dirt;                           // �ѱ��޸�(��)��־
};

// �����ϳ�����ṹ��������ǰ 8 ��һ��
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

// �ļ�Ŀ¼��ṹ 
struct dir_entry {                           // �ļ�Ŀ¼��ṹ    16 �ֽ�
 	unsigned short inode;              // i �ڵ��     2 �ֽ�
	char name[NAME_LEN];              // �ļ���     14 �ֽ�    �ļ�ϵͳ����ݸ������ļ����ҵ��� i �ڵ�ţ��Ӷ�ͨ�����Ӧ i �ڵ���Ϣ�ҵ��ļ����ڵĴ��̿�λ��
};

extern struct m_inode inode_table[NR_INODE];           // ���� i �ڵ������
extern struct file file_table[NR_FILE];                           // �ļ�������(64��)
extern struct super_block super_block[NR_SUPER];      // ����������(8��)
extern struct buffer_head * start_buffer;                     // ��������ʼ�ڴ�λ��
extern int nr_buffers;                                                 // �������

extern void check_disk_change(int dev);                    // ����������������Ƿ�ı�
extern int floppy_change(unsigned int nr);                 // ���ָ�������������̸��������������̸������򷵻� 1�����򷵻� 0
extern int ticks_to_floppy_on(unsigned int dev);        // ��������ָ������������ȴ�ʱ�� (���õȴ���ʱ��)
extern void floppy_on(unsigned int dev);                   // ����ָ��������
extern void floppy_off(unsigned int dev);                  // �ر�ָ������������
                                                                                   
extern void truncate(struct m_inode * inode);              // �� i �ڵ�ָ�����ļ���Ϊ 0  
extern void sync_inodes(void);                                   // ˢ�� i �ڵ���Ϣ
extern void wait_on(struct m_inode * inode);               // �ȴ�ָ���� i �ڵ�
extern int bmap(struct m_inode * inode,int block);              // �߼���(���Σ����̿�)λͼ������ȡ���ݿ� block ���豸�϶�Ӧ���߼����
extern int create_block(struct m_inode * inode,int block);    // �������ݿ� block ���豸�϶�Ӧ���߼��飬���������豸�ϵ��߼����
extern struct m_inode * namei(const char * pathname);       // ��ȡָ��·������ i �ڵ��
extern int open_namei(const char * pathname, int flag, int mode,      // ����·����Ϊ���ļ�������׼��
	struct m_inode ** res_inode);
extern void iput(struct m_inode * inode);                  // �ͷ�һ�� i �ڵ�(��д�豸)
extern struct m_inode * iget(int dev,int nr);              // ���豸��ȡָ���ڵ�ŵ�һ�� i �ڵ�
extern struct m_inode * get_empty_inode(void);       // �� i �ڵ��(inode_table)�л�ȡһ������ i �ڵ���
extern struct m_inode * get_pipe_inode(void);          // ��ȡ (����һ)�ܵ��ڵ㣬����Ϊ i �ڵ�ָ��(�����NULL��ʧ��)
extern struct buffer_head * get_hash_table(int dev, int block);    // �ڹ�ϣ���в���ָ�������ݿ飬�����ҵ���Ļ���ͷָ��
extern struct buffer_head * getblk(int dev, int block);                 // ���豸��ȡָ���� (���Ȼ��� hash ���в���)
extern void ll_rw_block(int rw, struct buffer_head * bh);             // ����д���ݿ�
extern void brelse(struct buffer_head * buf);                                // �ͷ�ָ���������� 
extern struct buffer_head * bread(int dev,int block);                     // ��ȡָ�������ݿ�
extern void bread_page(unsigned long addr,int dev,int b[4]);      // �� 4 �黺������ָ����ַ���ڴ���
extern struct buffer_head * breada(int dev,int block,...);                // ��ȡͷһ��ָ�������ݿ飬����Ǻ�����Ҫ���Ŀ�
extern int new_block(int dev);                               // ���豸 dev ����һ�����̿�(���Σ����̿�),�����߼����
extern void free_block(int dev, int block);               //  �ͷ��豸�������е��߼���(���Σ����̿�)block,��λָ���߼��� block ���߼���λͼ����λ
extern struct m_inode * new_inode(int dev);           // Ϊ�豸 dev ����һ���� i �ڵ㣬���� i �ڵ��
extern void free_inode(struct m_inode * inode);     // �ͷ�һ�� i �ڵ�(ɾ���ļ�ʱ)
extern int sync_dev(int dev);            // ˢ��ָ���豸������
extern struct super_block * get_super(int dev);     // ��ȡָ���豸�ͳ�����
extern int ROOT_DEV;                

extern void mount_root(void);         // ��װ���ļ�ϵͳ

#endif
