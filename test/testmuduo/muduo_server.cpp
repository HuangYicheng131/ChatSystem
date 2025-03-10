/*
提供了两个主要的类
TCPServer
TCPClient

epoll + 线程池
能够把网络I/O代码业务代码区分开
                用户的连接和断开    用户的可读写事件
*/

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <functional>
#include <string>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;

// 基于moduo网络库开发服务器程序
// 1. 组合TcpServer对象
// 2. 创建EventLoop事件循环对象的指针
// 3. 明确TcpServer构造函数需要什么参数，输出ChatServer的构造函数
// 4. 在当前服务器类的构造函数当中，注册处理连接和读写事件的回调函数
// 5. 设置合适的服务端线程数量，muduo库会自动划分I/O和worker线程
class ChatServer
{
public:
    ChatServer(EventLoop *loop,
               const InetAddress &listenAddr,                               // 事件循环
               const string &nameArg) :                                     // IP+Port
                                        _server(loop, listenAddr, nameArg), // 服务器的名字
                                        _loop(loop)
    {
        // 给服务器注册用户连接的创建和断开回调
        _server.setConnectionCallback(bind(&ChatServer::onConnection, this, _1));

        // 给服务器注册用户读写事件回调
        _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));

        // 设置服务器端的线程数量 1个I/O线程，1个worker线程
        _server.setThreadNum(2);
    }

    // 开启事件循环
    void start()
    {
        _server.start();
    }

private:
    // 专门处理用户的连接创建和断开 epoll listenfd accept
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() << "state: online" << endl;
        }
        else{
            cout << conn->peerAddress().toIpPort() << " -> " << conn->localAddress().toIpPort() << "state: offline" << endl;
            conn->shutdown(); // close(fd);
        }
    }

    // 专门处理用户的读写事件
    void onMessage(const TcpConnectionPtr &conn, // 连接
                   Buffer *buffer,               // 缓冲区
                   Timestamp receiveTime)        // 接收时间
    {
        string buf = buffer->retrieveAllAsString();
        cout << "recv data: " << buf << " time: " << receiveTime.toString() << endl;
        conn->send(buf); 
    }

    TcpServer _server; // #1
    EventLoop *_loop;  // #2 epoll
};

int main(){
    EventLoop loop; // epoll
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "CharServer");

    server.start();
    loop.loop(); // epoll_wait 以阻塞方式等待新用户连接，已连接用户的读写事件等操作

    return 0;
}