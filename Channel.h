#pragma once

#include "noncopyable.h"
#include "TimeStamp.h"
#include<functional>
#include<memory>
/*

理清楚 EventgLoop/Channel/Poller 之间的关系 -》Reactor模型上对应的 Demultiplex
Chanel 理解为通道，封装了sockfd和其他感兴趣的event 如EPOLLIN Epollout事件
还绑定了poller(轮询器)返回的具体事件

*/

class EventLoop;
class TimeStamp;

class Channel:noncopyable
{
public:
/*
1.对普通函数的调用：调用程序发出对普通函数的调用后，
程序执行立即转向被调用函数执行，直到被调用函数执行完毕后，再返回调用程序继续执行。
从发出调用的程序的角度看，这个过程为“调用-->等待被调用函数执行完毕-->继续执行”。

2、对回调函数调用：调用程序发出对回调函数的调用后，不等函数执行完毕，
立即返回并继续执行。这样，调用程序执和被调用函数同时在执行。
当被调函数执行完毕后，被调函数会反过来调用某个事先指定函数，
以通知调用程序：函数调用结束。这个过程称为回调（Callback）
*/
//重要--》内核检测到就绪的文件描述符的时候 将触发回调函数
   using EventCallback =std::function<void()>;//function函数对象 返回是void 不需要参数
   //EventCallback（）回调函数
   using  ReadEventCallback =std::function<void(TimeStamp)>;//读需要传时间参数的 返回为void的函数对象
   //ReadEventCallback(TimeStamp); 读需要读时间
   Channel(EventLoop *Loop,int fd);
   ~Channel();
   //fd得到poller通知以后，处理事件
   void handleEvent(TimeStamp receiveTime);
   //设置回调函数对象
   /*/std::move语句可以将左值变为右值而避免拷贝构造
   .什么是左值，什么是右值，
   简单说左值可以赋值，右值不可以赋值。
   以下面代码为例，“ A a = getA();”
   该语句中a是左值，getA()的返回值是右值。*/
   void setReadCallback(ReadEventCallback cb){ readCallback_=std::move(cb);}//给函数取值readCallback是一个函数对象
   void setWriteCallback(EventCallback cb){ writeCallback_=std::move(cb);}
   void setCloseCallback(EventCallback cb){ closeCallback_=std::move(cb);}
   void setErrorCallback(EventCallback cb){ errorCallback_=std::move(cb);}

   //防止当channel被手动remove掉了 ，Channel还在执行回调函数
   void tie(const std::shared_ptr<void>&);

   int fd() const{return fd_;}
   int events() const {return events_;}
   int set_revents(int revt){revents_=revt;}
   
//EPOLLIN = 0x001,events_的ReadEvent位
   void enableReading() {events_|=kReadEvent; update();}//a|=b 等效于 a=a|b 按位或并赋值 开始对read感兴趣
   void disableReading() {events_&=~kReadEvent; update();}//~是取反 不对read感兴趣了
   void enableWritinging() {events_|=kWriteEvent; update();}
   void disableWriting() {events_&=~kWriteEvent; update();}
   void disableAll() {events_=kNoneEvent; update();}

   bool isNoneEvent() const{return events_==kNoneEvent;}//fd感兴趣的事件是否是KNoneEvent
   bool isWriting() const{return events_&kWriteEvent;}
   bool isReading() const{return events_==kReadEvent;}

   int index(){return index_;}//这个是访问index_的
   void set_index(int idx){index_=idx;}
   //one loop per thread
   EventLoop* ownerloop(){return loop_;}
   void remove();
private:
    void update();
    void handleEventWithGuard(TimeStamp recevieTime);
    static const int kNoneEvent;//对无事件感兴趣
    static const int kReadEvent;//对读事件感兴趣
    static const int kWriteEvent;//对写事件感兴趣的
    
    EventLoop *loop_; // 事件public:
   
    const int fd_;//fd,Poller监听的对象  epoll_ctl 来添加
    int events_;//注册的fd感兴趣的事件 注意和下面事件的区别
    int revents_;//poller返回的具体发生的事件
    int index_;//被poller使用的

    std::weak_ptr<void> tie_;//智能指针
    bool tied_;

    //因为Channel通道里面可以获知fd最终发生的具体事件revents，所以它负责条用具体事件的回调函数
    ReadEventCallback readCallback_;//读回调
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

