#### ���幦��
- Ӳ��(�쳣)�жϴ�������ļ�    asm.s   traps.c 
- ϵͳ���÷���������ļ�      system_call.s     fork.c, sys.c, exit.c, signal.c 
- ���̵��ȵ�ͨ�ù����ļ�         schedule.c, panic.c, printk, vsprintf, mktime 




#### 8.1.3 ����ͨ�������
- schedule.c    �����ں˵�����Ƶ���� schedule(),sleep_on()��wakeup()���������ں˵ĺ��ĵ��ȳ������ڶԽ��̵�ִ�н����л���ı���̵�ִ��״̬�����⻹�����й�ϵͳʱ���жϺ�������������ʱ�ĺ���
- mktime.c      ������һ���ں�ʹ�õ�ʱ�亯�� mktime(), ���� init/main.c �б�����һ��
- panic.c         ����һ�� panic() �������������ں����г��ִ���ʱ��ʾ������Ϣ��ͣ��
- printk.c , vsprintf.c  ���ں���ʾ��Ϣ��֧�ֳ���ʵ�����ں�ר����ʾ���� printk() ���ַ�����ʽ��������� vsprintf() 



ϵͳ������ system_call.s   int 0x80
