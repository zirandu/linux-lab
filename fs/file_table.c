/*
 *  linux/fs/file_table.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <linux/fs.h>

struct file file_table[NR_FILE];   // �ļ������飬��64�� ���������ϵͳͬʱ����64���ļ�
//�ڽ��̵����ݽṹ(�����̿��ƿ��ƽ���������)�У�ר�Ŷ����б����̴��ļ����ļ��ṹָ������ filp[NR_OPEN]�ֶ�
//NR_OPEN=20,���ÿ����������ͬʱ��20���ļ���
//��ָ���������˳��ż���Ӧ�ļ���������ֵ�������ָ����ָ���ļ����д򿪵��ļ�����磬filp[0] ���ǽ��̵�ǰ���ļ������� 0 ��Ӧ���ļ��ṹָ�롣