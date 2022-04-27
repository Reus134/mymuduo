#pragma once

#include<vector>
#include "noncopyable.h"
#include "Channel.h"

#include <unordered_map>
#include "TimeStamp.h"

class Channel;
class EventLoop;
//muduo库中多路事件分发器的核心IO复用模块
class Poller:noncopyable
{
public:
    using ChannelList=std::vector<Channel*>;//ChannelList这个东西维护一个Channel*的数组
    Poller(EventLoop *loop);
    virtual ~Poller() = default;//默认的析构函数

    //给所有IO复用保留统一的接口
    virtual TimeStamp poll(int timeoutMs,ChannelList *activeChannels)=0;
    virtual void updateChannel(Channel *channel)=0;
    virtual void removeChannel(Channel *channel)=0;

    //判断参数channel是否在当前的poller中
    bool hasChannel(Channel *channel) const;
    //默认的 eventloop 可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop *loop);
    Poller();
protected://继承的是可以
//key是sockfd value:sockfd所属的channel的通道类型
    using ChannelMap=std::unordered_map<int,Channel*>;//int是fd 
    ChannelMap channels_;
private:
   EventLoop *ownerLoop_;//定义Poller所属的事件循环的EventLoop;
    /* data */

};

