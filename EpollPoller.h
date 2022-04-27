#pragma once 

#include "Poller.h"
#include<vector>
#include<sys/epoll.h>
#include "TimeStamp.h"

/*
epoll的使用
epoll_create
epoll_ctl add/mod/del
epoll_wait
*/

class TimeStamp;
class EpollPoller:public Poller//注意public
{
public:
    
    EpollPoller(EventLoop *loop);
    ~EpollPoller() override;
 
    //重写基类的方法
    TimeStamp poll(int timeoutMs,ChannelList *activeChannels);//超时时间
    void updateChannel(Channel *channel) override;//更新 类似与epoll_ctr 
    void removeChannel(Channel *channel) override;//之前监听的fd删除

private:

    static const int kInitEventListSize=16;//对下面初始化长度
    using EventList=std::vector<epoll_event>;//epoll_event代表一个结构体 代表发生的事件和数据
   //填写活跃的连接
    void fillActiveChannels(int numEvents,ChannelList *activeChannels) const;
    //更新channel通道
    void update(int operation,Channel *channel);//对channel里面的事件来updata
    
    int epollfd_;
    EventList events_;
};
