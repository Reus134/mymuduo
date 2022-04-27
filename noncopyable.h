#pragma once//防止头文件被重复包含
/*
noncopyable被继承以后，派生类对象可以正常的构造和析构，但是派生类对象无法进行拷贝构造和赋值
*/
class noncopyable{

public:
    noncopyable(const noncopyable&)=delete;//不可拷贝构造
    noncopyable& operator=(const noncopyable&)=delete;//不可赋值
protected:
    noncopyable()=default;
    ~noncopyable()=default;
};