#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
using namespace muduo;

#include <iostream>
#include <vector>
using namespace std;

ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的回调操作
ChatService::ChatService()
{
    // 用户
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGIN_OUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if(_redis.connect()){
        // 设置上报消息回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMMessage, this, _1, _2));
    }
}

// 服务器异常，业务重置方法
void ChatService::reset(){
    // 把所有用户的状态设置为offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的业务处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // LOG_INFO << "do reg service!";
    string name = js["name"];
    string password = js["password"];

    User user;
    user.setName(name);
    user.setPassword(password);

    json response;
    response["msgid"] = REG_MSG_ACK;

    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功

        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 登录业务 id password
// object relation map
// 业务层操作的都是对象 业务和数据解耦
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"];
    string password = js["password"];

    User user = _userModel.query(id);
    cout << "user.id: " << user.getId() << endl;
    cout << "user.password: " << user.getPassword() << endl;

    json response;
    response["msgid"] = LOGIN_MSG_ACK;

    if (user.getId() == id && user.getPassword() == password)
    {
        if (user.getState() == "online")
        {
            response["errno"] = 1;
            response["errmsg"] = "this account is being used, input another!";
            conn->send(response.dump());
        }
        else
        {            
            // 登录成功
            // 记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // redis订阅channel
            _redis.subscribe(id);
            
            // 更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);

            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty()){
                response["offlinemsg"] = vec;
                // 读取离线消息后，把离线消息删除
                _offlineMsgModel.remove(id);
            }

            // 查询该用户的好友信息
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty()){
                vector<string> vec2;
                for(User &user : userVec){
                    json js2;
                    js2["id"] = user.getId();
                    js2["name"] = user.getName();
                    js2["state"] = user.getState();
                    vec2.push_back(js2.dump());
                }
                response["friends"] = vec2;
            }

            // 查询用户的群组消息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if(!groupuserVec.empty()){
                vector<string> groupV;
                for(Group &group : groupuserVec){
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for(GroupUser &user : group.getUsers()){
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }

                response["groups"] = groupV;
            }

            conn->send(response.dump());

        }
    }
    else
    {
        // 登录失败
        response["errno"] = 2;
        response["errmsg"] = "Id or password is invalid!";
        conn->send(response.dump());
    }
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();

    User user;
    user.setId(userid);
    user.setState("offline");
    _userModel.updateState(user);
    
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end()){
            _userConnMap.erase(it);
        }
    }

    // redis取消订阅
    _redis.unsubscribe(userid);
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;

    // 用户更新的状态信息
    if(user.getId() != -1){
        user.setState("offline");
        _userModel.updateState(user);
    }

    // redis取消订阅
    _redis.unsubscribe(user.getId());

    {
        lock_guard<mutex> lock(_connMutex);
        
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == conn)
            {
                user.setId(it->first);
                // 从 map 中删除用户连接信息
                _userConnMap.erase(it);
                break;
            }
        }
    }    
}

 // 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int toId = js["to"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toId);
        if(it != _userConnMap.end()){
            // 对方在线，转发消息
            it->second->send(js.dump());
            return;
        }
    }

    // 查询toid是否在线
    User user = _userModel.query(toId);
    if(user.getState() == "online"){
        _redis.publish(toId, js.dump());
        return;
    }

    // 离线，存储消息
    _offlineMsgModel.insert(toId, js.dump());
}

// 添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组的信息
    Group group(-1, name, desc);
    if(_groupModel.createGroup(group)){
        // 存储创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"];
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群聊业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupsUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for(int id : useridVec){
    
        auto it = _userConnMap.find(id);
        
        if(it != _userConnMap.end()){
            // 转发群消息
            it->second->send(js.dump());
        }
        else{
            // 查询toid是否在线
            User user = _userModel.query(id);
            if(user.getState() == "online"){
                _redis.publish(id, js.dump());
                return;
            }
            else{
                // 离线，存储消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    } 
}

void ChatService::handleRedisSubscribeMMessage(int userid, string msg){

    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end()){
        it->second->send(msg);
        return;
    }

    // 离线，存储消息
    _offlineMsgModel.insert(userid, msg);
}