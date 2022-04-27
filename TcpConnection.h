#pragma once

//主要是打包通信链路的（已经建立连接的客户端）
#include "noncopyable.h"
#include "InetAddress.h"
#include"Callbacks.h"
#include"Buffer.h"
#include "TimeStamp.h"

#include<memory>
#include <string>
#include<atomic>

class Channel;
class EventLoop;
class Socket; 

/*
TcpServer=>Acceptor=>有一个新用户连接，通过accept函数拿到connfd 
=》TcpConnection 设置回调操作=》channel=》Poller(注册回调)=》Channel的回调操作
*/
class TcpConnection:noncopyable,
               public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,
                const std::string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop()const {return loop_;}
    const std::string name() const {return name_;}
    const InetAddress& localAddress()const {return localAddr_;}
    const InetAddress& peerAddress()const {return peerAddr_;}
    
    bool connected() const {return state_==kConnected;}

    //发送数据
   void send(const std::string &buf);
    //关闭当前连接
    void shutdown();

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback& cb)
    { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }
    
    //连接建立了
    void connectionEstablished();
    //连接销毁
    void connectDestroyed();

private:
    enum StateE {kDisconnected,kConnecting,kConnected,kDisconnecting};
    void setState(StateE state) { state_ = state; }

    void handleRead(TimeStamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data,size_t len);
    void shutdownInLoop();

    //这里绝对不是mainLoop 因为TCPconnection 都是在subloop里管理的 mainloop只管接收新用户的连接
    EventLoop *loop_;
    const std::string name_;
    std::atomic_int state_;
    bool reading_;
    // 这里和Acceptor类似   Acceptor=》mainLoop    TcpConenction=》subLoop
    std::unique_ptr<Socket>socket_;
    std::unique_ptr<Channel>channel_;

    const InetAddress localAddr_;//当前主机的地址
    const InetAddress peerAddr_;//对端的

    ConnectionCallback connectionCallback_;//有新连接时的回调
    MessageCallback messageCallback_;//有读写消息的回调
    WriteCompleteCallback writeCompleCallback_;//消息发送完成以后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;

    size_t highWaterMark_;

    Buffer inputBuffer_;//接收数据的缓冲区  接收我读fd的 然后写缓存区
    Buffer outputBuffer_;//发送数据的缓冲区  发送我读缓存区的 写到fd上
};


