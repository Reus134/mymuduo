#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <string>


static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop==nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection is null",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop,
                const std::string& nameArg,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr)
                :loop_(CheckLoopNotNull(loop)),
                name_(nameArg)
                ,state_(kConnected)
                ,reading_(true)
                ,socket_(new Socket(sockfd))
                ,channel_(new Channel(loop,sockfd))
                ,localAddr_(localAddr)
                ,peerAddr_(peerAddr)
                ,highWaterMark_(64*1024*1024)//超过24M就超过水位线了
                {
                    //下面给Channel 设置相应的回调函数，Poller给channel通知感兴趣的事件发生了 channel会回调相应的回调函数
                    channel_->setReadCallback(
                        std::bind(&TcpConnection::handleRead,this,std::placeholders::_1)
                    );

                    channel_->setWriteCallback(
                        std::bind(&TcpConnection::handleWrite,this)
                    );

                    channel_->setCloseCallback(
                        std::bind(&TcpConnection::handleClose,this)
                    );

                    channel_->setErrorCallback(
                        std::bind(&TcpConnection::handleError,this)
                    );
                      LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
                      socket_->setKeepAlive(true);
                      
                }

TcpConnection::~TcpConnection()
{
     LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", 
        name_.c_str(), channel_->fd(), (int)state_);
}
//发送数据 数据=》json pb
void TcpConnection::send(const std::string &buf)
{
    if(state_==kConnected)
    {
        if(loop_->isInLoopThread())//在当前的线程里面
        {
            sendInLoop(buf.c_str(),buf.size());

        }
        else
        {
            loop_->runInLoop(std::bind(
                &TcpConnection::sendInLoop,this,
                buf.c_str(),buf.size()
            ));
  
        }
    }

}

/*
*发送数据 应用写的快（非阻辙IO）而内核发送数据慢 需要把代发送的数据写入缓冲区 而且设置了水位回调
*/ 
void TcpConnection::sendInLoop(const void* data,size_t len)
{
    ssize_t nwrote=0;
    ssize_t remaining=len;
    bool faultError=false;
    if(state_==kDisconnected)//如果shutdown了
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }
    //channel第一次开始写数据，而且缓冲区没有代发送的数据
    if(!channel_->isWriting()&&outputBuffer_.readableBytes()==0)
    {
        //不是发缓存区的数据哦
        nwrote=::write(channel_->fd(),data,len);//直接把data发送到fd（注意这里的readablebytes==0 所以缓存区没有要读的数据）
        if(nwrote>=0)
        {
            remaining=len-nwrote;//还剩下的
            if(remaining==0&&writeCompleCallback_)
            {
                //既然在这里数据就全部发送完成，就不用在给cahnnel设置epollout事件了
                loop_->queueInLoop(
                    std::bind(writeCompleCallback_,shared_from_this())
                );

            }
        
        }
         else // nwrote < 0  ->错误 
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) // SIGPIPE  RESET
                {
                    faultError = true;
                }
            }
        }
    }
    //没有发生错误 当前这一次write，并没有全部把数据发送出去  
    //剩余的数据需要保存到缓冲区中，然后给channel注册epollout事件（写事件 写道缓存区里面）
    //poller发现tcp的发送缓冲区（outputbuffer）有空间 会通知相应的sock—channel,调用handlewrite回调方法
    //最终就是调用TCponnection::handlewrite方法，把发送缓冲区的数据全部发送完成
    if(!faultError&&remaining>0)
    {
        //目前发送缓冲区深剩余的代发送数据的长度
        size_t oldLen=outputBuffer_.readableBytes();//现在发送缓冲区有读区 读了发送出去
        //if主要判断水位
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_//之前没事 +个remainning就超出水位线了
            && highWaterMarkCallback_)
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen+remaining)
            );
        }
        //调用完之后 添加到缓存区当中
        outputBuffer_.append((char*)data+nwrote,remaining);//复制到缓存区
        if(!channel_->isWriting())
        {
            channel_->enableWritinging();//注册channel的写事件 否则Poller不会给channel通知epollout
        }
    }

}

//关闭当前连接
void TcpConnection::shutdown()
{
    if(state_==kConnected)
    {
        setState(kDisconnecting);//正在断开
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));
    }

}   

void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())//说明outputBuffer中的数据已经全部完成 如果没有发送完就不会进这个if
    {
        //EPOLLHUP 表示读写都关闭 会触发回调
        socket_->shutdownWrite();//关闭写端 这个操作会触发EPOLLHUP 然后调用closecallback函数

    }

}
//连接建立了
void TcpConnection::connectionEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());//绑定
    channel_->enableReading();//像poller注册chanel的epollin事件

    //新连接建立了 ，执行回调
    connectionCallback_(shared_from_this());

}
//连接销毁
void TcpConnection::connectDestroyed()
{
    if(state_=kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();//把chanenl中所有感兴趣的事件 从poller中del掉
        connectionCallback_(shared_from_this());//连接断开
    }
    channel_->remove();//把Channel从Poller中删除掉

}

void TcpConnection::handleRead(TimeStamp receiveTime)
{
   int saveErrno=0;
   ssize_t n=inputBuffer_.readFd(channel_->fd(),&saveErrno);//先读
   if(n>0)
   {
       //已经建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
       //shared_from_this()是一个指向当前对象的智能指针
        messageCallback_(shared_from_this(),&inputBuffer_,receiveTime);//读到了就执行用户定义的callback
   }
   else if(n==0)//说明客户端断开
   {
     handleClose();
   }
   else
   {
       errno=saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
   } 
}

void TcpConnection::handleWrite()
{
    //对写事件感兴趣了
    if(channel_->isWriting())
    {
        int savedErrno=0;
        ssize_t n=outputBuffer_.writeFd(channel_->fd(),&savedErrno);//channel的fd往缓存区写
        if(n>0)
        {
            outputBuffer_.retrieve(n);//更新buffer
            if(outputBuffer_.readableBytes()==0)
            {
                channel_->disableWriting();
                if(writeCompleCallback_)//如果有回调 就执行回调(这个是用户定制的)
                {
                    //唤醒loop_对应的thread线程，执行回调
                    loop_->queueInLoop(
                        std::bind(writeCompleCallback_, shared_from_this())
                    );
  
                }
                if(state_==kDisconnecting)//正在关闭
                {
                    shutdownInLoop();//正在关闭也关闭
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->fd());//channel不能写
    }

}
//poller=>channel::closeCallback =>TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());//指向TcpConnection的指针
    connectionCallback_(connPtr);//执行连接关闭的回调
    closeCallback_(connPtr);//关闭连接的回调 执行的是Tcpserver::removeConnection方法

}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen =sizeof optval;
    int err=0;
    if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen)<0)
    {
        err=errno;
    }
    else
    {
        err=optval;
    }

    LOG_ERROR("TcpConnection::handleError name:%s -SO_ERROR:%d \n",name_.c_str(),err);

}


