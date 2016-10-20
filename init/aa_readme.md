        ���ں�Դ����� init/ Ŀ¼��ֻ��һ�� main.c �ļ���ϵͳ��ִ���� boot/ Ŀ¼�е� head.s �����ͻὫִ��Ȩ���� main.c,�ó�����Ȼ��������ȴ�������ں˳�ʼ�������й�����

 #### 7.1.1 ��������
        main.c ��������������ǰ�� setup.s ����ȡ�õ�ϵͳ��������ϵͳ�ĸ��ļ��豸���Լ�һЩ�ڴ�ȫ�ֱ�����
        ��Щ�ڴ����ָ�������ڴ�Ŀ�ʼ��ַ��ϵͳ��ӵ�е��ڴ���������Ϊ���ٻ������ڴ��ĩ�˵�ַ�������������������(RAMDISK),�����ڴ��ʵ����١�
        �����ڴ��ӳ��ʾ��ͼ��

 | �ں˳��� | ���ٻ��� | ������ |             ���ڴ���               |
 
- ���ٻ��岿�ֻ�Ҫ�۳����Դ��ROM BIOS ռ�õĲ��֣����ٻ��������ڴ��̵ȿ��豸��ʱ������ݵĵط�����1K(1024)�ֽ�Ϊһ�����ݿ鵥λ
- ���ڴ�������ڴ����ģ�� mm ͨ����ҳ���ƽ��й������䣬�� 4K �Լ�Ϊһ���ڴ�ҳ��λ
- �ں˳���������ɷ��ʸ��ٻ����е����ݣ�����Ҫͨ�� mm ����ʹ�÷��䵽���ڴ�ҳ��
    
        Ȼ���ں˽������з����Ӳ����ʼ�����������������š����豸���ַ��豸�� tty,�������˹����õ�һ������(task 0),�����г�ʼ��������ɺ����������ж�����
 ��־�Կ����жϣ����л������� 0 �����С�
        
        �������ں���ɳ�ʼ�����ں˽�ִ��Ȩ�л������û�ģʽ(����0)��Ҳ��CPU�� 0 ��Ȩ���л����˵� 3 ��Ȩ������ʱ main.c ��������͹��������� 0 �У�Ȼ��ϵͳ
��һ�ε��ý��̴������� fork(),������һ���������� init() ���ӽ���(ͨ������Ϊ init ����)

```
idle����(����0���)
����0(����0):     ϵͳ��ʼ��  -->  �������ڴ�����ֽ��й��ܻ�������� -->  ϵͳ�����ֳ�ʼ�������������� 0 ��ʼ��  -->  �Ƶ����� 0 ��ִ��  -->  �������� 1 (init)  --> ����ʱִ�� pause() 
                                                                                                                                                                                                                                |
                                  -------------------------------------------------------------------------------------------------------------
                                  |
����1(init����):   ���ظ��ļ�ϵͳ  -->   �����ն˱�׼ IO  -->  �������� 2 -->  ѭ���ȴ����� 2 �˳�   -->  �����ӽ���   -->  ѭ���ȴ������˳�   --     <----
                                                                                             |                                |                               |       |                                         |            |
                                  ----------------------------------         --------------                               <---------------<-----------             |
                                  |                                                                   |                                                               |                                                      |
����2:               ���붨�� rc �ļ�    -->    ִ�� shell ���� rc   -->   �˳�                                                            |                                                      |
                                                                                                                                                                      |                                                      |                                                         
                                                                                                                                                                      |                                                      |
����n:                                                                                                                                                    �����ն˱�־IO  -->  ִ�� shell --> �˳� -->
```

- main.c ��������ȷ����η���ʹ��ϵͳ�����ڴ棬Ȼ������ں˸����ֵĳ�ʼ�������ֱ���ڴ�������жϴ��������豸���ַ��豸�����̹����Լ�Ӳ�̺�����Ӳ�����г�ʼ��������
- �˺󣬳�����Լ�"�ֹ�"�ƶ������� 0 (����0)�����У���ʹ fork() �����״δ���������1 (init����)���������е��� init() �������ú����г��򽫼�������Ӧ�û����ĳ�ʼ����ִ�� shell ��¼����
��ԭ���� 0 �����ϵͳ����ʱ������ִ�У���˽��� 0 ͨ��Ҳ����Ϊ idle ���̡���ʱ���� 0 ��ִ�� pause() ϵͳ���ã����ֻ���õ��Ⱥ�����

- init() �����Ĺ��ܿɷ�Ϊ4������:
   (1)��װ���ļ�ϵͳ
   (2)��ʾϵͳ��Ϣ
   (3)����ϵͳ��ʼ��Դ�����ļ� rc �е�����
   (4)ִ���û���¼ shell ����

      �������ȵ���ϵͳ���� setup(),�����ռ�Ӳ���豸��������Ϣ����װ���ļ�ϵͳ���ڰ�װ���ļ�ϵͳ֮ǰ��ϵͳ�����ж��Ƿ���Ҫ�Ƚ��������̡��������ں�ʱ�����������̵Ĵ�С������ǰ���ں˳�ʼ��
�������Ѿ�������һ���ڴ����������̣����ں˾ͻ����ȳ��԰Ѹ��ļ�ϵͳ���ص��ڴ���������С�
        Ȼ�� init() ��һ���ն��豸 tty0, ���������ļ��������Բ�����׼���� stdin����׼��� stdout �� ������� stderr �豸���ں����������Щ���������ն�����ʾһЩϵͳ��Ϣ��������ٻ������л���
�����������ڴ��������ڴ����ֽ����ȡ�

- 0�Ž��̾��ǳ�ʼ���̣�����ִ�� main ������������� 1 �Ž������� init,�Ժ�Ĺ������� 1 �Ž����������

#### ��ϵͳģʽת���û�ģʽ
        �ڵ��� move_to_user_mode ֮ǰ��main ����һֱ��ϵͳ״̬�����У������Ժ�Ľ�����Ӧ�����û�ģʽ�����еģ����Ҫ�ڵ��� fork() ֮ǰ�����������ģʽת��Ϊ�û�ģʽ��
 ������ģ���жϷ��أ�����֪�����û�ģʽ��ʱ����������жϻ����ϵͳ���жϷ�������жϷ��������������ϵͳģʽ�µģ���˻ᷢ��ջת�������жϷ��ص�ʱ�򣬻�ָ���
 �ж�֮ǰ��״̬����Ͱ����˽�����ģʽת��Ϊ�û�ģʽ���жϵĴ���������ѹջ���ѣ�������ǿ�������ѹջ��ģ���жϣ�Ȼ����һ������ָ��������ֳ��ָ��Ĺ�����

