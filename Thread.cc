#include"Thread.h"
#include "CurrentThread.h"

#include<semaphore.h>//信号量

 std::atomic_int Thread::numCreated_(0);//类外的成员变量需要单独先进行初始化

 Thread::Thread(ThreadFunc func,const std::string &name)
     :started_(false)
     ,joined_(false)
     ,tid_(0)
     ,func_(std::move(func))
     ,name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
   //线程要启动起来 join-》就是普通的工作线程 
   //！join 就不是工作线程 就把他设置成一个分离的线程
   //内核资源会自动回收的 不会出现孤儿线程
   if(started_&&!joined_)
   {
       thread_->detach();//thread_是一个指针
   }
}

void Thread::start()//一个Thread对象就是记录他一个线程的详细信息
{
    started_=true;
    /*
    函数原型 ：int sem_init(sem_t *sem, int pshared, unsigned int value);
    参数解释 ：
    sem ：指向信号量对象
    pshared : 指明信号量的类型。当为1时，用于进程；当为0时，用于线程。
    value : 指定信号量值的大小
    返回值：成功返回0，失败时返回-1，并设置errno。
    作用：创建信号量，并为信号量值赋初值。*/
    sem_t sem;//sem信号量
    sem_init(&sem,false,0);
     // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]()
    {
        //这个线程就在干下面三个事情
        // 获取线程的tid值
        tid_ = CurrentThread::tid();
        /*返回值：成功返回0，失败时返回-1，并设置errno。
         作用: 以原子操作的方式为将信号量增加1*/
        sem_post(&sem);//lambada表达式 以引用获取外部变量
        // 开启一个新线程，专门执行该线程函数
        func_(); //执行线程函数
    }));
    //这里必须等待获取上面新创建的线程的tid值
    /*作用: 以阻塞的方式等待信号量，当信号量的值大于零时，
    执行该函数信号量减一，当信号量为零时，调用该函数的线程将会阻塞
    。*/
    sem_wait(&sem);
}

void Thread::join()
{
  joined_=true;
  thread_->join();
}
void Thread::setDefaultName()
{
    int num=++numCreated_;
    if(name_.empty())
    {
        char buf[32]={0};
        // 将num按照format的格式格式化为字符串，然后再将其拷贝至buf中。
        snprintf(buf,sizeof(buf),"Thread%d",num);
        name_=buf;
    }
}