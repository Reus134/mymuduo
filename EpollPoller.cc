#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>

const int kNew=-1;//新的 还没有加入  是channel的成员index_=-1;
const int kAdded=1;//已经加入了
const int kDeleted=2;//已经被删除了

EpollPoller::EpollPoller(EventLoop *loop)
    :Poller(loop)
    /*是 ::EPOLL_CLOEXEC 这样就可以在某些情况下解决掉一些问题 
    即在fock后关闭子进程中无
    用文件描述符的问题 即fork创建的子进程在子进程中关闭该socket*/
    ,epollfd_(::epoll_create1(EPOLL_CLOEXEC))//全局作用域符号：当全局变量在局部函数中与其中某个变量重名，那么就可以用::来区分如：
    ,events_(kInitEventListSize)//发生事件的EVENT
{
    if(epollfd_<0)//创建失败了
    {
        LOG_FATAL("epoll_create error:%d \n",errno);
    }
}

EpollPoller::~EpollPoller() 
{
   ::close(epollfd_);
}
 
 //主要是epoll_wait
TimeStamp EpollPoller::poll(int timeoutMs,ChannelList *activeChannels)//超时时间
{
    //实际上应该用LOG_DEBUG输出日志更为合适
    LOG_INFO("func=%s =>fd total count:%lu \n",__FUNCTION__,channels_.size());
    int numEvents = ::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);  //*events_.begin()=vector底层的头地址
    int saveErrno = errno;
    
    TimeStamp now(TimeStamp::now());//创建一个事件  构造函数 现在的事件传进去

    if(numEvents>0)
    {
        LOG_INFO("%d events happened \n",numEvents);
        fillActiveChannels(numEvents,activeChannels);//填写活跃的channel
        if(numEvents==events_.size())//如果监听的event 数量已经等于event_s(初始的eventlist) 那么扩容
        {
          events_.resize(events_.size()*2);//变为原来的两倍
        }
    }
    else if(numEvents==0)//超时； 
    {
        LOG_DEBUG("%s timeout! \n",__FUNCTION__);
    }
    else//这个就是错误类型； 
    {
       if(saveErrno!=EINTR)//!=外部中断
       {
           errno=saveErrno;//重置成当前发生的errno
           LOG_ERROR("EPollPoller::poll() err!");
       }
    }
   return now;//返回现在的时间

}
//channel update remove=>EventLoop updateChannel removeChnnel =>poller(IO复用的接口)
/*
*            EventLoop
       ChannelList      Poller
                      channelMap <fd,channel*>
*/
void EpollPoller::updateChannel(Channel *channel) //更新 类似与epoll_ctr 
{
     const int index=channel->index();
     LOG_INFO("func=%s => fd=%d events=%d index=%d \n",__FUNCTION__,channel->fd(),channel->events(),index);
     if(index==kNew||index==kDeleted)//就是说这个channel里面没有这个fd 删除了也是没有fd
     {
         if(index==kNew)//fd从来没有被添加过
         {
          int fd=channel->fd();
          channels_[fd]=channel;//一个map
         }
        channel->set_index(kAdded);//添加了
        
         update(EPOLL_CTL_ADD,channel);;//channel往EPOLL里面注册
     }
     else//channel已经在poller上注册过了
     {
         int fd=channel->fd();
         if(channel->isNoneEvent())
         {
             update(EPOLL_CTL_DEL,channel);
             channel->set_index(kDeleted);
         }
        else//这就是说对某些事件是感兴趣的
        {
            update(EPOLL_CTL_MOD, channel);//要修改一下channel
        }
     }
}
//从poller中删除channel
void EpollPoller::removeChannel(Channel *channel)//之前监听的fd删除
{
    int fd=channel->fd();
    int index=channel->index();
    channels_.erase(fd);//channelmap 删除fd

    LOG_INFO("func=%s => fd=%d events=%d index=%d \n",__FUNCTION__,channel->fd(),channel->events(),index);
    
    if(index==kAdded)//如果是加入了这个EPOLL的内核队列的话
    {
       update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);
}
//填写活跃的连接
void EpollPoller::fillActiveChannels(int numEvents,Poller::ChannelList *activeChannels) const
{
   for(int i=0;i<numEvents;++i)//把这么多个channel
   {
       Channel *channel=static_cast<Channel*>(events_[i].data.ptr);//events_[i].data.ptr是void*
       channel->set_revents(events_[i].events);//正在发生的event 设置
       activeChannels->push_back(channel);//EventLoop拿到的色它的Poller给他返回的正在发生的所有史前的俄channel列表了
   }
}
//更新channel通道 epoll_ctl add/mod/del;
void EpollPoller::update(int operation,Channel *channel)
{
    epoll_event event;//创建event
    memset(&event,0,sizeof(event));//清0;

    event.events=channel->events();//epoll事件
    int fd=channel->fd();
    event.data.fd=fd;
    event.data.ptr=channel;
    

    if(::epoll_ctl(epollfd_,operation,fd,&event)<0)//<0就是出错了 正常就会把这个EVENT加入到内核的队列里面
    {
         if(operation==EPOLL_CTL_DEL)
         {
             LOG_ERROR("epoll_ctl del error：%d\n",errno);
         }
         else
         {
             LOG_FATAL("epoll_ctl add/mod error：%d\n",errno);
         }
    }


}