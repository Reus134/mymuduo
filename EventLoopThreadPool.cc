#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include <memory>


EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseloop,const std::string &nameArg)
    :baseLoop_(baseLoop_)
    ,name_(nameArg)
    ,started_(false)
    ,numThreads_(0)
    ,next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{}

void EventLoopThreadPool::start(const ThreadInitCallback &cb )//默认值只需要声明时给就可以； 
{
   started_=true;
   for(int i=0;i<numThreads_;++i)
   {
       char buf[name_.size()+32];
       snprintf(buf,sizeof(buf),"%s%d",name_.c_str(),i);
       EventLoopThread *t=new EventLoopThread(cb,buf);
       threads_.push_back(std::unique_ptr<EventLoopThread>(t));
       loops_.push_back(t->startLoop());//底层创建线程 绑定一个新的EventLoop 并返回该Loop的地址

   }
   //整个服务器端只有一个线程 运行者base_loop 并且这个cb 用户给的回调函数不为空
   if(numThreads_==0&&cb)
   {
     cb(baseLoop_);
   }

}

// 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop=baseLoop_;
    
    if(!loops_.empty())//循环取loop  通过轮询
    {
        loop=loops_[next_];
        ++next_;
        if(next_>=loops_.size())
        {
            next_=0;
        }
    }
    return loop;//如果没有创建其他的loop 底层用的永远是baseloop；

}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
   if(loops_.empty())
    {
        return std::vector<EventLoop*>(1,baseLoop_);
    }
    else 
    {
        return loops_;
    }
}

