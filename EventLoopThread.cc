#include "EventLoopThread.h"
#include "EventLoop.h"
#include<string>

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
    const std::string &name)
    :loop_(nullptr)
    ,exiting_()
    ,thread_(std::bind(&EventLoopThread::threadFunc,this),name)//创建我们自己封装的Thread
    ,mutex_()
    ,cond_()
    ,callback_(cb)
{

}

EventLoopThread::~EventLoopThread()
{
  exiting_=true;
  if(loop_!=nullptr)
  {
    loop_->quit();//线程绑定的loop也要退出
    thread_.join();//等待Thread 结束
  }
}

EventLoop* EventLoopThread::startLoop()//开启循环  获取这个新线程 并把这个新线程的EventLoop返回
{
    //启动线程 执行的是func() 
    //也就是上文构造函数绑定的EventLoopThread::threadFunc()
   thread_.start();
   
   EventLoop *loop =nullptr;//注意作用遇 和下面的loop不是同一个
   {
     std::unique_lock<std::mutex>lock(mutex_);
     while(loop_==nullptr)//就是说下面的这个 threadFunc()还没有执行到loop_=&loop;
     {
     cond_.wait(lock);//等待在这把互斥锁上
     }
     loop=loop_;
   }
   return loop;

}

//这个方法是在单独的新线程里面执行的 
void EventLoopThread::threadFunc()
{
    //oneloop onethread
    EventLoop loop ;//每个线程先创建一个独立的eventLOOP，和上面的线程一一对应的

    if(callback_)
    {
        callback_(&loop);//就调用这个回调
    }
    
    {
        std::unique_lock<std::mutex>lock(mutex_);//锁
        loop_=&loop;//是当前这个loop
        cond_.notify_one();
    }
    loop.loop();//执行EventLoop的loop 开启事件循环来监听远端用户的连接
    std::unique_lock<std::mutex>lock(mutex_);//不玩了 不进行事件循环了 因为上面是会一直循环的
    loop_=nullptr;

}