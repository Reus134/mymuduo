#include "TimeStamp.h"
#include<time.h>
TimeStamp::TimeStamp():microSecondsSinceEpoch_(0)//默认构造
{}

TimeStamp::TimeStamp(int64_t microSecondsSinceEpoch)
: microSecondsSinceEpoch_(microSecondsSinceEpoch)
{

}

TimeStamp TimeStamp::now()//现在的时间
{
    return TimeStamp(time(NULL));
}
std::string TimeStamp::toString() const//只读方法
{
  char buf[128]={0};

  tm *tm_time=localtime(&microSecondsSinceEpoch_);//一个指向tm的指针
  snprintf(buf,128,"%4d/%02d/%02d %02d:%02d:%02d",//年月日 和时间
  tm_time->tm_year+1900,
  tm_time->tm_mon+1,
  tm_time->tm_mday,
  tm_time->tm_hour,
  tm_time->tm_min,
  tm_time->tm_sec
  );
  return buf;
}
// #include <iostream>
// int main()
// {
//     std::cout << Timestamp::now().toString() << std::endl; 
//     return 0;
// }