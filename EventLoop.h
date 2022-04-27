#pragma once

#include "noncopyable.h"
#include "TimeStamp.h"
#include "CurrentThread.h"

#include <functional>
#include<atomic>
#include<vector>

#include<memory>
#include <sys/eventfd.h>
#include<mutex>



class Channel;
class Poller;
class TimeStamp;

//事件循环类 主要包含了两个大块 Channel Poller(epoll的抽象)
//EVENTLOOP相当与Reactor模型里面的 Reactor模块       
class EventLoop:noncopyable
{
public:
  using Functor=std::function<void()>;//回调函数
  EventLoop();
  ~EventLoop();
   //开启事件循环
  void loop();
  //退出事件循环
  void quit();

  TimeStamp pollReturnTime() const {return pollReturnTime_;}
   //在当前的loop下执行
  void runInLoop(Functor cb);
  //把CB放入队列中，唤醒loop所在的线程，执行cb
  void queueInLoop(Functor cb);

  void wakeup();
  //EventLoop的方法=》Poller的方法
  void updateChannel(Channel *channel);
  void removeChannel(Channel *channel);
  bool hasChannel(Channel *channel);
  //判断EventLoop对象是否在自己的线程里面
  bool isInLoopThread() const{return threadId_==CurrentThread::tid();}

private:
  void handleRead();//wake up
  void doPendingFunctors();//执行回调
  using ChannelList =std::vector<Channel*>;
/*在多线程操作中，使用原子变量之后就不需要再使用互斥量来保护该变量了，
用起来更简洁。因为对原子变量进行的操作只能是一个原子操作（atomic operation），
原子操作指的是不会被线程调度机制打断的操作，这种操作一旦开始，就一直运行到结束，
中间不会有任何的上下文切换。多线程同时访问共享资源造成数据混乱的原因
就是因为 CPU 的上下文切换导致的，使用原子变量解决了这个问题，
因此互斥锁的使用也就不再需要了。
*/
  std::atomic_bool looping_;//原子操作  
  std::atomic_bool quit_;//标识退出循环

  const pid_t threadId_;//记录当前loop所在的线程ID

  TimeStamp pollReturnTime_;//poller返回发生时间的Channels的时间点
  std::unique_ptr<Poller> poller_;

  int wakeupFd_;  //主要作用是 当mainLoop获取一个新的用户的Channel,通过轮询算法选择一个subloop，通过该成员唤醒Subloop处理channel
  std::unique_ptr<Channel>wakeupChnannel_;//通知唤醒工作线程

  ChannelList activeChannels_;
  

  std::atomic_bool callingPendingFunctors_; //标识当前loop是否需要执行的回调操作
  std::vector<Functor>pendingFunctors_;//存储loop需要执行的所有回调操作
  std::mutex mutex_;//互斥锁用来保护上面vector容器的线程安全操作

};


                