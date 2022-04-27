#pragma once 

#include <vector>
#include<string>
#include <algorithm>
/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///readerIndex是用户发到服务器的 然后把他存到缓存区
/// 
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
//网络库底层的缓存区
class Buffer
{
public:
/*size_t中的t是type（类型）的意思。size_t的意思是size of type，即某种类型的大小（字节数）。

size_t是C内部预定义的一个类型：

typedef unsigned int size_t

因此这句代码相当于：unsigned int size=sizeof(long long);

而sizeof()函数的功能就是求变量在内存中所占的字节数。

因此，这句话的意思是将long long类型在内存中所占的字节数赋值给无符号整型变量size。*/
    static const size_t kCheapPrepend=8;//看上面的图  一开始8个字节 是prependable bytes
    static const size_t kInitialSize=1024;
    explicit Buffer(size_t initialSize=kInitialSize)
    :buffer_(kCheapPrepend+initialSize)
    ,readerIndex_(kCheapPrepend)
    ,writeIndex_(kCheapPrepend)
    {}
   

    size_t readableBytes() const
    {
        return writeIndex_-readerIndex_;//可读的 是中间部分
    }

    size_t writableBytes() const
    {
       return buffer_.size()-writeIndex_;
    }
    size_t prependableBytes() const{
        return readerIndex_;
    }

    //返回缓冲区中可读数据的起始地址
    const char* peek() const
    {
        return begin()+readerIndex_;
    }

    void retrieve(size_t len)
    {
        if(len<readableBytes()) //len小了 缓存区里面还有数据没有读
        {
            readerIndex_+=len;//在应用读去了缓存区的一部分readIndex_前进len的距离
          
        }
        else
        {
            retrieveAll(len);
        }
    
    }
    void retrieveAll(size_t len)
    {
        readerIndex_=writeIndex_=kCheapPrepend;//读完了 复位了
        
    }
    //把onMessage 函数上报的Buffer数据 转换成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());//应用可读取数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(),len);
        retrieve(len);
        return result;
    }
    void ensureWriteableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len); // 扩容函数
        }
    }
    void append(const char *data,size_t len)//往data指针所指的地方加
    {
        ensureWriteableBytes(len);
        //添加数据[data,data+len] 拷贝到begin()+writeIndex_->可写区的起点
        std::copy(data,data+len,beginWrite());
        writeIndex_+=len;
    }

    char* beginWrite()
    {
        return begin()+writeIndex_;
    }

    const char* beginWrite() const
    {
        return begin()+writeIndex_;
    }
    //从fd读取数据
    ssize_t readFd(int fd,int* saveErrno);
    //通过fd发送数据
    ssize_t writeFd(int fd,int* saveErrno);
    
private:
    char* begin()
    {
        //it.operator*() vector开头元素本身所指的地址
        return &*buffer_.begin();
    }
    const char* begin() const
    {
        //it.operator*() vector开头元素本身所指的地址
        return &*buffer_.begin();
    
    }

    void makeSpace(size_t len)
    {
        //writableBytes() + prependableBytes()-kCheapPrepend < len 这样好理解一些
         //prependableBytes()-kCheapPrepend是前面已经读了的 空闲了的
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)//prependableBytes() 是已经读了的
        {
            buffer_.resize(writeIndex_ + len);//调整缓存区的大小 直接在后面家一个len长度

        }
        else//加上之前已经空闲的pr死则ependableBytes()-kCheapPrepend是足够你len读取的
        {
            //连到一起
            size_t readable=readableBytes();
            //[begin()+readerIndex_, begin()+writeIndex_]
                 
            std::copy(begin()+readerIndex_,
                  begin()+writeIndex_,
                  begin()+kCheapPrepend);
                  readerIndex_=kCheapPrepend;//读指针指向8；
                  writeIndex_=readerIndex_+readableBytes();//现在是可写的了
                

        }
    }

    std::vector<char>buffer_;
    size_t readerIndex_;
    size_t writeIndex_;

};


