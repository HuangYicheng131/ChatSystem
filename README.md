# C++ Cluster Chat System

在Linux环境下基于muduo、mysql、redis开发的C++集群聊天服务器和客户端。实现用户注册、用户登录、添加好友、创建群组、添加群组、好友聊天、群组聊天、离线消息、用户注销等功能。

## 项目技术

- 基于 muduo 网络库开发网络核心模块，实现高效通信
- 使用第三方 JSON 库实现通信数据的序列化和反序列化
- 封装 MySQL 接口，将用户数据储存到磁盘中，实现数据持久化
- 基于 CMake 构建项目
- 使用 Nginx 的 TCP 负载均衡功能，将客户端请求分派到多个服务器上，以提高并发处理能力
- 基于发布-订阅的服务器中间件redis消息队列，解决跨服务器通信问题

# 服务器端

将服务器端分为网络模块、业务模块和数据模块。

## 网络模块

使用 muduo 开发网络模块。Muduo 网络库底层实质上为 Linux 的 epoll + pthread 线程池，且依赖 boost 库，**优点是能够将网络 I/O 的代码和业务代码分开**。

muduo的网络设计核心为**一个线程一个事件循环**，有一个 `main Reactor` 负载 accept 连接，然后把连接分发到某个 `sub Reactor`，该连接的所用操作都在该 `sub Reactor` 所处的线程中完成。多个连接可能被分派到多个线程中，以充分利用 CPU，reactor poll 的大小是固定的，根据CPU的数目确定。

muduo 库服务器编程流程：
1. 组合 TcpServer 对象
2. 创建 EventLoop 事件循环对象的指针，可以向 loop 上注册感兴趣的事件，相应事件发生 loop 会上报给我们
3. 明确 TcpServer 构造函数需要的参数，输出服务器对应类的构造函数
4. 在当前服务器类的构造函数中，注册处理连接断开的回调函数和处理读写事件的回调函数
```
 // 注册连接回调
_server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

// 注册消息回调
_server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

// 设置线程数量
_server.setThreadNum(2);
```
5. 设置合适的服务器端线程数量，muduo 会自动分配 I/O 线程与工作线程
6. 开启事件循环 start()

## 业务模块

主要通过 ChatService 类实现

```
// 处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp time)>;

// 存储消息 id 和对应的业务处理方法
unordered_map<int, MsgHandler> _msgHandlerMap;

// 存储在线用户的通信连接
unordered_map<int, TcpConnectionPtr> _userConnMap;

// 定义互斥锁，保证 _userConnMap 的线程安全
mutex _connMutex;

// 数据操作类对象
UserModel _userModel;
OfflineMsgModel _offlineMsgModel;
FriendModel _friendModel;
GroupModel _groupModel;
```

### 消息类型

```
REG_MSG = 1, // 注册消息
REG_MSG_ACK, // 注册响应消息

LOGIN_MSG, // 登录消息
LOGIN_MSG_ACK, // 登录响应消息
LOGIN_OUT_MSG, // 登出消息

ONE_CHAT_MSG, // 聊天消息
ADD_FRIEND_MSG, // 添加好友消息

CREATE_GROUP_MSG, // 创建群组
ADD_GROUP_MSG, // 加入群组
GROUP_CHAT_MSG, // 群聊天
```

### 登录业务

如果用户 ID 存在、密码正确且用户处于离线状态：

1. 将连接加入到 `_userConnMap` 中
2. 将用户状态更新为在线（通过 `User` 类实现）
3. 查询离线信息（通过 `OfflineMsgModel` 类实现）
4. 查询好友信息（通过 `FriendModel` 类实现）
5. 查询群聊信息（通过 `GroupModel` 类实现）

## 数据模块

### User

对应于数据库中的 `User` 关系表

```
int _id;
string _name;
string _password;
string _state;
```

### UerModel

对 `User` 表进行操作

1. User表插入: `insert`
2. 根据id查询用户信息: `query`
3. 更新用户状态信息: `updateState`
4. 重置用户状态信息: `resetState`

### FriendModel

对 `Friend` 表进行操作

1. 添加好友关系: `insert`
2. 返回用户好友列表: `query`

### GroupUser

是 `User` 类的派生类，记录了用户在群聊中的角色：`creator` 和 `normal`

### Group

记录群聊信息，ID、名称、描述、成员

```
 int _id;
string _name;
string _desc;
vector<GroupUser> users;
```

### GroupModel

对 `AllGroup` 和 `GroupUser` 关系表进行操作

1. 创建群组: `createGroup`
2. 加入群组: `addGroup`
3. 查询用户所在群组信息: `queryGroups`，用户登录时返回群聊消息
4. 根据指定的userid查询群组用户id列表，除userid自己，主要用于群聊业务给其他组员群发消息: `queryGroupsUsers`，发群聊消息时调用

### OfflineMsgModel

对 `OfflineMessage` 表进行操作，加入、移除、查询离线消息

## 数据库

### User

| Field    | Type                     | Null | Key | Default | Extra          |
|----------|--------------------------|------|-----|---------|----------------|
| id       | int                      | NO   | PRI | NULL    | auto_increment |
| name     | varchar(50)              | NO   | UNI | NULL    |                |
| password | varchar(50)              | NO   |     | NULL    |                |
| state    | enum('online','offline') | YES  |     | offline |                |

### Friend

| Field    | Type | Null | Key | Default | Extra |
|----------|------|------|-----|---------|-------|
| userid   | int  | NO   | PRI | NULL    |       |
| friendid | int  | NO   | PRI | NULL    |       |

### AllGroup

| Field     | Type         | Null | Key | Default | Extra          |
|-----------|--------------|------|-----|---------|----------------|
| id        | int          | NO   | PRI | NULL    | auto_increment |
| groupname | varchar(50)  | NO   |     | NULL    |                |
| groupdesc | varchar(200) | YES  |     |         |                |

### GroupUser

| Field     | Type                     | Null | Key | Default | Extra |
|-----------|--------------------------|------|-----|---------|-------|
| groupid   | int                      | NO   | PRI | NULL    |       |
| userid    | int                      | NO   | PRI | NULL    |       |
| grouprole | enum('creator','normal') | YES  |     | normal  |       |

## nginx

负载均衡器
1. 把 client 请求按照负载算法分发到具体业务服务器 ChatServer 上
2. 能够和 ChatServer 保持心跳机制，监测 ChatServer 故障
3. 能够发现新添加的 ChatServer 设备，方便扩展服务器数量

客户端的请求和服务器的响应都需要经过负载均衡器

配置 TCP 负载均衡，编译时需要加入 `--with-stream` 参数激活
```
./configure --with-stream 
make && make install
```

配置文件 `conf/nginx.conf`
```
stream {
    upstream MyServer {
        server 127.0.0.1:6000 weight=1 max_fails=3 fail_timeout=30s;
        server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
    
        server {
            proxy_connect_timeout 1s;
            listen 8000; # nginx 监听的端口号
            proxy_pass MyServer;
            tcp_nodelay on;
        }
    }
}
```

启动 nginx，重新加载配置
```
./nginx # 启动 nginx
./nginx -s reload # 重载配置文件
```

## redis

基于发布/订阅的消息队列，解决跨服务器通信问题，降低服务器集群间的耦合程度

redis 支持多种不同的客户端编程语言，C++对应的时hiredis，从 github 上下载客户端，进行源码编译安装

具体功能在 `Redis` 类中实现，连接、发布、订阅、取消订阅、设置回调函数。在独立的子线程中接收订阅通道的消息，以免阻塞主线程。

# 客户端

利用socket编程与服务器端建立连接

主线程用于向服务器发送数据，创建子线程用于接收和处理服务器发送的数据，并通过信号量控制主子线程间的通信

```
// 初始化信号量
sem_init(&rwsem, 0, 0);
// 连接服务器成功，启动子线程，接收
std::thread readTask(readTaskHandler, clientfd);
readTask.detach();
```

用户根据菜单输入要执行的业务，并跳转到相应代码执行。向服务器发送数据后，主线程阻塞，等待子线程接收并处理服务器响应数据，再通知主线程继续执行。

## 登录后业务

`g_isLoginSuccess` 记录用户登录状态，当用户成功登录后，循环显示主菜单，根据输入的命令调用响应的处理函数

```
// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令, 格式help"},
    {"chat", "一对一聊天, 格式chat:friendid:message"},
    {"addfriend", "添加好友, 格式addfriend:friendid"},
    {"creategroup", "创建群组, 格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组, 格式addgroup:groupid"},
    {"groupchat", "群聊, 格式groupchat:groupid:message"},
    {"loginout", "注销, 格式loginout"}};

// 系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};
```

若用户注销登录，改变 `isMainMenuRunning`，退出主菜单循环，返回登录注册页面
