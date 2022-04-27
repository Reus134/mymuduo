#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include<functional>
#include <strings.h>

using namespace std::placeholders;
EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop==nullptr)
    {
        LOG_FATAL("%s:%s:%d mainloop is null",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}


TcpServer::TcpServer(EventLoop *loop,
    const InetAddress &ListenAddr,const std::string &nameArg,
    Option option)
    :loop_(CheckLoopNotNull(loop))
    ,ipPort_(ListenAddr.toIpPort())
    ,name_(nameArg)
    ,acceptor_(new Acceptor(loop,ListenAddr,option==kReusePort))
    ,threadPool_(new EventLoopThreadPool(loop,name_))
    ,connectionCallback_()
    ,messageCallback_()
    ,nextConnId_(1)
    ,started_(0)
    {
        //当有新用户连接时 会执行TcpServer::newConnection回调
        acceptor_->setNewConnectionCallback(std::bind
        (&TcpServer::newConnection,this,_1,_2));
    }

TcpServer:: ~TcpServer()
{
    for(auto &item:connections_)
    {
        //TcpConcetion 这个局部的shared_ptr对象
        // 出右括号，可以自动施放new出来的Tcpconnction对象资源了
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed,conn)//在loop里面销毁资源
        );
    }
}
    
//设置顶层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
   threadPool_->setThreadNum(numThreads);
}
//开启服务器监听
void TcpServer::start()
{
     if(started_++==0)//防止一个TcpServer对象被start多次
     {
         threadPool_->start(threadcallback_);//启动底层的loop线程池
         //在这个mainloop 里面开始listen看有没有新的连接了
         loop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get()));//acceptor_.get()相当于this指针绑定Acceptor;
     }
}
//有一个新的客户端连接，acceptor会执行这个回调操作
void TcpServer::newConnection(int sockfd,const InetAddress &peerAddr)
{
    EventLoop *ioLoop=threadPool_->getNextLoop();//轮询 返回一个loop
    char buf[64]={0};
    /*
    snprintf()，函数原型为int snprintf(char *str, size_t size, const char *format, ...)。
    将可变参数 “…” 按照format的格式格式化为字符串，然后再将其拷贝至str中
    */
    snprintf(buf,sizeof buf,"-%s#%d",ipPort_.c_str(),nextConnId_);
    ++nextConnId_;//连接的名称
    std::string connName=name_+buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
        
    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    //根据连接成功的sockfd 创建Tcpnnection连接对象
     
    TcpConnectionPtr conn(new TcpConnection(
                            ioLoop,
                            connName,
                            sockfd,   // Socket Channel
                            localAddr,
                            peerAddr));
    connections_[connName] = conn;
    //下面的回调都是用户设置给TCpserver => Tcpconnection=>channel=>poller=>notify channel 调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleCallback_);
   
    // 设置了如何关闭连接的回调   conn->shutDown()
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection,this,std::placeholders::_1)
    );
    // 直接调用TcpConnection::connectEstablished(连接建立成功了)ioLoop就是给到的下一个loop
    ioLoop->runInLoop(std::bind(&TcpConnection::connectionEstablished,conn));//conn相当于this
   
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn)
    );

}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", 
        name_.c_str(), conn->name().c_str());
    connections_.erase(conn->name());//从哈希表中删除

    EventLoop *ioLoop=conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn)
    );



}