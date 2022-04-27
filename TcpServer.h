#pragma once

/*
用户使用muduo编写服务器程序 你先把头文件在这里包含了
*/
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TimeStamp.h"
#include  "TcpConnection.h"

#include<functional>
#include<string>
#include<memory>
#include<atomic>
#include<unordered_map>


//对外的服务器编程使用的类
class TcpServer:noncopyable
{
public:
    using  ThreadInitCallback=std::function<void(EventLoop*)>;


    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop,
    const InetAddress &ListenAddr,const std::string &nameArg,
    Option option=kNoReusePort);
    ~TcpServer();
    
    void setThreadInitcallback(const ThreadInitCallback &cb){threadcallback_=cb;}
    void setConnectionCallback(const ConnectionCallback &cb){connectionCallback_=cb;}
    void setMessageCallback(const MessageCallback &cb){messageCallback_=cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback &cb){writeCompleCallback_=cb;}
    
    //设置顶层subloop的个数
    void setThreadNum(int numThread);
    //开启服务器监听
    void start();
private:
    void newConnection(int sockfd,const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);
    
    using ConnectionMap=std::unordered_map<std::string,TcpConnectionPtr>;

    EventLoop *loop_;//用户自己定义的loop ->baseloop
    //服务器的端口号和名称
    const std::string ipPort_;
    const std::string name_;

    //mainLoop里面的Acceptor 任务就是监听新的连接事件
    std::unique_ptr<Acceptor> acceptor_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;//Oneloop per thread；池管理loop 非常经典
    
    ConnectionCallback connectionCallback_;//有新连接时的会代哦
    MessageCallback messageCallback_;//有读写消息的回调
    WriteCompleteCallback writeCompleCallback_;//消息发送完成以后的回调

    ThreadInitCallback threadcallback_;//loop线程初始化

    std::atomic_int started_;
    
    int nextConnId_;
    ConnectionMap connections_; // 保存所有的连接

};

