#### 总体功能
- 硬件(异常)中断处理程序文件    asm.s   traps.c 
- 系统调用服务处理程序文件      system_call.s     fork.c, sys.c, exit.c, signal.c 
- 进程调度等通用功能文件         schedule.c, panic.c, printk, vsprintf, mktime 




#### 8.1.3 其他通用类程序
- schedule.c    包括内核调用最频繁的 schedule(),sleep_on()和wakeup()函数，是内核的核心调度程序，用于对进程的执行进行切换或改变进程的执行状态，另外还包括有关系统时钟中断和软盘驱动器定时的函数
- mktime.c      仅包含一个内核使用的时间函数 mktime(), 仅在 init/main.c 中被调用一次
- panic.c         包含一个 panic() 函数，用于在内核运行出现错误时显示出错信息并停机
- printk.c , vsprintf.c  是内核显示信息的支持程序，实现了内核专用显示函数 printk() 和字符串格式化输出函数 vsprintf() 



系统调用在 system_call.s   int 0x80
