#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>         
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>


static int createNonblocking()
{
    int sockfd=::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
    if(sockfd<0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
}

Acceptor::Acceptor(EventLoop *loop,const InetAddress &listenAddr,bool reuseport)
    :loop_(loop)
    ,acceptSocket_(createNonblocking())//创建Socket 只需要fd
    ,acceptChannel_(loop,acceptSocket_.fd())
    ,listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    /*TCPServer::start() Acceptor.listen 有新用户连接 
    执行回调（connfd打包成一个channel)给到subloop 由subloop来做读写事件
    */
   //baseloop监听到acceptChanne_(listenfd)有事件发生后
   acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead,this));
}

Acceptor::~Acceptor()
{
acceptChannel_.disableAll();//不再关心了
acceptChannel_.remove();//删除
}

void Acceptor::listen()
{
   listenning_=true;
   acceptSocket_.listen();//监听socket
   acceptChannel_.enableReading();//acceptChannel_要注册到Poller里面
}
//Chnnal收到读事件之后 listenfd 有事件发生了 ，就是有新用户连接了
void Acceptor::handleRead()
{
  InetAddress peerAddr;
  int connfd =acceptSocket_.accept(&peerAddr);
  if(connfd>0)//监听到了
  {
    if(newConnectionCallback_)
    {
        newConnectionCallback_(connfd,peerAddr);//轮询找到subloop 唤醒分发当前客户大und俄channel
    }
    else{
        ::close(connfd);
    }
  }
  else
  {
    LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    if(errno==EMFILE)
    {
        LOG_ERROR("%s:%s:%d sockfd reached limit err! \n", __FILE__, __FUNCTION__, __LINE__);
    }
  }
}