# -Algorithm-and-data-structure
自定义的msg queue，使用linux系统mutex互斥锁和cond条件变量实现阻塞队列。因为不仅仅信号量，
共享内存、消息队列在NDK下都不能用，所以之前使用Linux 下IPC的消息队列，msgget/msgsnd/msgrcv都不能使用，
所以没有办法，只能自己实现消息队列，采用linux 下互斥锁和条件变量实现了读时-队列空-会阻塞，写时-队列满-会阻塞。
