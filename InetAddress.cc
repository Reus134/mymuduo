   #include"InetAddress.h"
   #include<string.h>
   #include<strings.h>
   
    InetAddress::InetAddress(uint16_t port,std::string ip)//默认地址 第一种是传端口和 IP
    {
        bzero(&addr_,sizeof(addr_));//先清零
       addr_.sin_port=htons(port);//host to net s
       //inet_addr 点分十进制转换成长整形 并且是网络字节序
       addr_.sin_addr.s_addr=inet_addr(ip.c_str());//ip.c_str()是从char*转换
       addr_.sin_family=AF_INET;
    }
    std::string InetAddress::toIp() const//只打印加const
    {
      //打印IP地址
      char buf[64]={0};
      ::inet_ntop(AF_INET,&addr_.sin_addr,buf,sizeof(buf));//用buf来接收
      return buf;

    }
    std::string InetAddress::toIpPort() const
    {
      //ip:port
      char buf[64]={0};
      ::inet_ntop(AF_INET,&addr_.sin_addr,buf,sizeof(buf));//用buf来接收
      size_t end=strlen(buf);//buf的有效长度
      uint16_t port =ntohs(addr_.sin_port);
      sprintf(buf+end,":%u",port);
      return buf;
    }
    uint16_t InetAddress::toPort() const
    {
    return ntohs(addr_.sin_port);
    }

// #include<iostream>
// int main()
// {
//     InetAddress addr(8080);
//     std::cout<<addr.toIp()<<" : "<<addr.toPort()<<std::endl;
//     std::cout<<std::endl;
    
//     std::cout<<addr.toIpPort()<<std::endl;
//     return 0;
// }


   