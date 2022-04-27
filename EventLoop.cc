#include "EventLoop.h"
#include "Poller.h"
#include "Channel.h"
#include "Logger.h"

#include <errno.h>
#include<fcntl.h>
#include<memory>
#include<unistd.h>
#include <sys/eventfd.h>

//防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisThread =nullptr;
 
const int kPollTimeMs=10000;//定义默认的Poller IO复用的超时时间 10s

//创建wakeupfd 用来唤醒subReactor来处理新来的Channel;
int createEventfd()
{
     //eventfd，是用来实现多进程或多线程的之间的事件通知的，
     //也可以由内核通知用户空间应用程序事
     //evtfd 是線程間通信
    int evtfd=::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);//唤醒子线程
    if(evtfd<0)
    {
        LOG_FATAL("Eventfd error:%d \n",errno);
        
        return evtfd;
    }
}
EventLoop::EventLoop():
    looping_(false),
    quit_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),//参数需要一个EventLoo[类型的]
    wakeupFd_(createEventfd()),
    wakeupChnannel_(new Channel(this,wakeupFd_))
    //先没有的
{
    LOG_DEBUG("EventLoop created %p in thread %d \n",this,threadId_);//%P地址
    if(t_loopInThisThread)//说明这个线程有一个loop
    {
        LOG_FATAL("ANOTHER EventLoop %p exists in this thread %d \n",t_loopInThisThread,threadId_);

    }
    else{
        t_loopInThisThread=this;
    }
    //设置wakeupfd的事件类型以及发生事件后的回调操作
    //相当于 function<void(TempStemp) readCallback_=std::bind(&EventLoop::handleRead,this);
    //bind函数只是绑定了函数 还没有传参数 readcallback_的时候得自己传Timestamep类型
    wakeupChnannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    //每一个Eventloop都将监听wakeupchannel的EPOLLIN读事件
    wakeupChnannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChnannel_->disableAll();
    wakeupChnannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread=nullptr;//当前线程没有Loop了
}

 //开启事件循环
  void EventLoop::loop()
  {
    looping_=true;
    quit_=false;//两个标志
    LOG_INFO("EventLoop %p start looping \n",this);

    while(!quit_)
    {
     activeChannels_.clear();//每次进来先把这个vector clear掉
     //监听2类fd 1。client fd 2。wakeupfd
     pollReturnTime_=poller_->poll(kPollTimeMs,&activeChannels_);//超时时间传进去   poll一直会阻塞
      for(Channel *channel:activeChannels_)//遍历这些活跃的事件
      {
          //Poller监听哪些channel发生事件，然后上报给EventLoop,通知Channel处理相应的事件
          channel->handleEvent(pollReturnTime_);//看看发生了读事件 关闭事件 还是什么事件
      }
      /*
      IO线程 mainLOOP 主要接收新用户的连接 用 Channel打包新用户的fd  给subLoop
      mainLoop 实现注册一个回调CB （需要subloop去执行） subloop在睡觉阻塞着呢
      wakeup 以后 执行下面的方法 执行mainloop注册的cb（回调函数）
      */
      doPendingFunctors();//执行当前EventLoop事件循环需要处理的回调操作；
    }
    LOG_INFO("Eventloop %p stop looping.\n",this);
    looping_=false;
  }
  //退出事件循环 1.Loop在自己的线程中  2.在非loop的线程中调用LOOP的quit
  void EventLoop::quit()
  {
  quit_=true;

  /*
              MainLoop

              （muduo库之间是没有的）=============生产者-消费者线程安全队列（mainLoop往里面放 subloop在里面拿）
 
               （通过wakeupfd来进行通讯的）
    subloop1    subloop2     subloop3

    如果在SUBloop2 想把subloop1给退出了 需要i把他唤醒
   
  */
    if(!isInLoopThread())//如果是在其它线程中调用的quit,在一个subloop(工作线程 )中调用了mainLoop的quit
     {
     wakeup();//先把主线程唤醒
     }
  }

   //在当前的loop下执行cb
void EventLoop::runInLoop(Functor cb)
{
   if(isInLoopThread())//在当前线程中
   {
    cb();//在当前线程中就直接执行回调
   }
   else//在非当前loop线程中执行CB，就需要唤醒loop所在线程执行CB
   {
   queueInLoop(cb);
   }
}
  //把CB放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{  
  
  {
  std::unique_lock<std::mutex>lock(mutex_);//智能锁
  pendingFunctors_.emplace_back(cb);//emplace_back是直接构造 push_back()是拷贝构造
  }
  //唤醒需要执行上面回调操作的loop线程
  //这里的callingPendingFunctors代表正在执行回调 
  //loop又有了新的回调 执行完之后就又会阻摄在poll  所以需要唤醒
  if(!isInLoopThread()||callingPendingFunctors_)
  {
      wakeup();//唤醒loop所在线程
  }

}
//結合eventfd來的 只要一直有向eventfd 写东西 读到了 就唤醒了
void EventLoop::handleRead()
{
    //读了一个8字节的数据
    uint64_t one = 1;                                 //长整数 8个字节
    ssize_t  n   = read(wakeupFd_,&one,sizeof(one));  //wakeupfd 读取出来
    if(n!=sizeof(one)) //n如果不是8个字节
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu  bytes instead of 8",n);
    }
}
//用来唤醒  像wakefd_写一个数据,wakeupChannel就发生读事件 就被唤醒了 写一个东西 他上面就有东西可读了
void EventLoop::wakeup()
{
  uint64_t one=1;
  ssize_t n=write(wakeupFd_,&one,sizeof(one));//写东西 唤醒
  if(n!=sizeof(one))
  {
      LOG_ERROR("EventLoop ::wakeup() %lu bytes instead of 8\n",n);
  }
}
  //EventLoop的方法=》Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
 poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
   poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel)
{
   return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()
{
    //Functor不执行完 是不能够
    std::vector<Functor>functors;
    callingPendingFunctors_=true;//这个bool值代表需要执行回调函数
    {
       std::unique_lock<std::mutex> lock(mutex_);
        //把之前需要的回调换到局部vector里面 ，那么pendingFunctors就解放了
        functors.swap(pendingFunctors_);//用锁来控制,swap没有执行完不能退出 
       }

    for(const Functor &functor:functors)
    {
     functor();//遍历  执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_=false;


}