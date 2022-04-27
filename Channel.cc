#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>
#include "TimeStamp.h"
#include"Logger.h"

class Timestamp;
const int Channel::kNoneEvent=0;//对无事件感兴趣
const int Channel::kReadEvent=EPOLLIN|EPOLLPRI;//EPOL连接到达；有数据来临；EPOLLPRI外带数据
const int Channel::kWriteEvent=EPOLLOUT;//对写事件感兴趣的

//EventLoop:ChannelList Poller
Channel::Channel(EventLoop *loop,int fd):
loop_(loop)
,fd_(fd),
events_(0)//初始对任何事情都不感兴趣,revents_(0)
,index_(-1)
,tied_(false){}//构造函数

Channel::~Channel()
{

}
//channel的tie方法什么时候调用过一个TcpConnection新连接创建的时候  TcpConnection=>Channel
//回调都在TcpConnection里面 如果TcpConnection被析构了 那么 
 void Channel::tie(const std::shared_ptr<void>& obj)
 {
  tie_=obj;//智能指针
  tied_=true;//绑定过了 tied_就置为true;
 }

//当改变Channel所表示的fd的events事件后，update负责在poller里面更改事件Epoll_ctl;
//EventLoop=>ChannelList Poller(包含这两个的意思)
 void Channel::update()
 {
  //通过Channel 所属的EventLoop调用poller的相应方法 注册fd的Event事件
   //add code...
   loop_->updateChannel(this);//通知上面的EventLoop EventLoop 在来通知Poller
 }
//在Channel所属的EventLoop中 把当前的channel删除
 void Channel::remove()
 {
     //add code...
  loop_->removeChannel(this);
 }
//fd得到poller通知后 处理事件的
 void Channel::handleEvent(TimeStamp receiveTime)
 {
    if(tied_)
    {
    std::shared_ptr<void> guard=tie_.lock();
    if(guard)
    {
        handleEventWithGuard(receiveTime);//调用下面的
    }
    }
    else{
        handleEventWithGuard(receiveTime);
    }

 }
 //根据poller监听的具体事件（revents_是poller返回的具体发生的事件）
 void Channel::handleEventWithGuard(TimeStamp recevieTime)
 { 
     LOG_INFO("channel handleEvent revents:%d\n",revents_);
     //EPOLLIN ：表示对应的文件描述符可以读； 可读的时候对方挂断了
    if((revents_& EPOLLHUP)&&!(revents_ &EPOLLIN))//EPOLLHUP：表示对应的文件描述符被挂断；
    {
     if(closeCallback_)
     {
         closeCallback_();
     }
    }
    if(revents_&EPOLLERR)
    {
        if(errorCallback_)
        {
           errorCallback_();
        }
    }
    if(revents_&(EPOLLIN|EPOLLPRI))//EPOLLPRI：表示对应的文件描述符有紧急的数据可读 in是可读
    {
        if(readCallback_)
        {
            readCallback_(recevieTime);//发生了读事件 就处理回调
        }
    }
    if(revents_&EPOLLOUT)
    {
        if(writeCallback_)
        {
            writeCallback_();
        }
    }
 }