#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include<functional>
#include<mutex>
#include<condition_variable>
#include<string>

class EventLoop;

class EventLoopThread:noncopyable
{
public:
    using ThreadInitCallback=std::function<void(EventLoop*)>;
    EventLoopThread(const ThreadInitCallback &cb=ThreadInitCallback(),
    const std::string &name=std::string());
    ~EventLoopThread();

    EventLoop* startLoop();//开启循环
private:

    void threadFunc();

    EventLoop *loop_;
    bool exiting_;//跟互斥所相关的
    Thread thread_;//大写的是自己封装的thread类
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};


