#include "Poller.h"
#include "EventLoop.h"
#include"Channel.h"

class Channel;
class EventLoop;

Poller::Poller(EventLoop* loop) :ownerLoop_(loop){}

bool Poller::hasChannel(Channel *channel) const
{
   
   auto it=channels_.find(channel->fd());//ChannelMap channels_; 哈系表里面赵key
   return it!=channels_.end()&&it->second==channel;//不等于->找到了 等于->没找到 第二个是有这个文件描述符 那么他的channel是否在
}

//static Poller* newDefaultPoller(EventLoop *loop); 
//如果实现这个类 Poller是基类 上面这个函数需要EPOLLER 是继承的类 设计不太好