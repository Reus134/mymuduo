#pragma once

#include<iostream>
#include<string>
class TimeStamp
{
public:
  TimeStamp();
  explicit TimeStamp(int64_t microSecondsSinceEpoch);//禁止隐式类型转换
  static TimeStamp now();//现在的时间
  std::string toString() const;//只读方法
private:
  int64_t microSecondsSinceEpoch_;
};