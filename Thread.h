#pragma once

#include "noncopyable.h"

#include<functional>
#include<thread>
#include<memory>
#include <unistd.h>
#include<string>
#include<atomic>

//我感觉这个类就是自己封装了一个Thread 类 底层是C+=11的thread
class Thread:noncopyable
{
public:
    using ThreadFunc=std::function<void()>;//如果你想传参数 应该用绑定器传


    explicit Thread(ThreadFunc func,const std::string &name=std::string());
    ~Thread();

    void start();
    void join();

    bool started() const {return started_;}//是否开始了
    pid_t tid() const {return tid_;}//
    const std::string& name() const {return name_;}
    
    static int numCreated() { return numCreated_; }
private:
    
    void setDefaultName();

    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;//要用一个智能指针来控制 因为这个线程类一创建线程就会开始工作
    pid_t tid_;//线程的名字
    ThreadFunc func_;//function 绑定的
    std::string name_;//线程的，名字
    static std::atomic_int numCreated_;



};


