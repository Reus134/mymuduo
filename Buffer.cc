#include "Buffer.h"
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/*
   从fd上读数据  Poller工作在LT模式
   Buffer缓冲区是有大小的！但是从fd上读数据的时候却不知道  TCP数据的最终大小
*/
//read是将读入的数据（写到）散布读到writable bytes缓冲区中  读了外部传来的数据传入到缓存区中
ssize_t Buffer::readFd(int fd,int* saveErrno)
{
     char extrabuf[65536]={0};//栈上内存空间 64k的空间
     
      //struct iovec {
       //        void  *iov_base;    /* Starting address */
        //       size_t iov_len;     /* Number of bytes to transfer */
       //    };
     struct iovec vec[2];
     const size_t writable=writableBytes();//缓存取底层剩余的可写空间的大小
     vec[0].iov_base=begin()+writeIndex_;
     vec[0].iov_len=writable;//这个0是缓存区上的 

     vec[1].iov_base=extrabuf;
     vec[1].iov_len=sizeof extrabuf;

     const int iovcnt =(writable<sizeof extrabuf)?2:1;//不够64k的空间 就选择2
     const ssize_t n=::readv(fd,vec,iovcnt);
     if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable) // Buffer的可写缓冲区已经够存储读出来的数据了
    {
        writeIndex_ += n;//直接改变可写区的
    }
    else // extrabuf里面也写入了数据   拷贝到缓存区
    {
        writeIndex_ = buffer_.size();//移到底了
        append(extrabuf, n - writable);  // writerIndex_开始写 n - writable大小的数据
    }

    return n;

}

//写给客户端 要从readableBytes缓存区读取数据
ssize_t Buffer::writeFd(int fd,int* saveErrno)
{
    ssize_t n=::write(fd,peek(),readableBytes());
    if(n<0)
    {
        *saveErrno=errno;
    }
    return n;
}


