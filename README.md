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

muduo的网络设计核心为一个线程一个事件循环，有一个 `main Reactor` 负载 accept 连接，然后把连接分发到某个 `sub Reactor`，该连接的所用操作都在该 `sub Reactor` 所处的线程中完成。多个连接可能被分派到多个线程中，以充分利用 CPU，reactor poll 的大小是固定的，根据CPU的数目确定。

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