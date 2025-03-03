#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户好友列表信息
vector<User> g_currUserFriendList;
// 记录当前登录用户群组列表信息
vector<Group> g_currUserGroupList;
// 显示当前登录成功用户的基本信息
void showCurrentUserData();
// 控制主菜单页面
bool isMainMenuRunning = false;


// 接收线程
void readTaskHandler(int clientfd);
// 获取事件（聊天信息需要添加时间信息）
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析命令行参数ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // main线程用于接收用户输入，负责发送数据
    for (;;)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "====================" << endl;
        cout << "1. register" << endl;
        cout << "2. login" << endl;
        cout << "3. quit" << endl;
        cout << "====================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读取缓冲区中的回车

        switch (choice)
        {
        case 1: // register
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50);
            cout << "password:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (-1 == len)
            {
                cerr << "send reg msg error:" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    cerr << "recv reg response error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>())
                    { // 注册失败
                        cerr << name << " already exits, register error!" << endl;
                    }
                    else
                    { // 注册成功
                        cout << name << " register success, userid is " << responsejs["id"] << ", do not forget it!" << endl;
                    }
                }
            }
        }
        break;
        case 2: // login
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get();
            cout << "password:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (-1 == len)
            {
                cerr << "send log msg error:" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    cerr << "recv reg response error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>())
                    { // 登录失败
                        cerr << responsejs["errmsg"] << endl;
                    }
                    else
                    {
                        // 记录当前用户的id和name
                        g_currentUser.setId(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);

                        // 记录当前用户的好友列表
                        if (responsejs.contains("friends"))
                        {
                            vector<string> vec = responsejs["friends"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currUserFriendList.push_back(user);
                            }
                        }

                        // 记录当前用户的群组消息列表
                        if (responsejs.contains("groups"))
                        {
                            vector<string> vec = responsejs["groups"];
                            for (string &groupstr : vec)
                            {
                                json grpjs = json::parse(groupstr);
                                Group group;
                                group.setId(grpjs["id"].get<int>());
                                group.setName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);

                                vector<string> vec2 = grpjs["users"];
                                for (string &userstr : vec2)
                                {
                                    GroupUser user;
                                    json js = json::parse(userstr);
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);
                                    group.getUsers().push_back(user);
                                }

                                g_currUserGroupList.push_back(group);
                            }
                        }

                        // 显示登录用户的基本信息
                        showCurrentUserData();

                        if (responsejs.contains("offlinemsg"))
                        {
                            vector<string> vec = responsejs["offlinemsg"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                int msgtype = js["msgid"].get<int>();
                                if(ONE_CHAT_MSG == msgtype){
                                    cout << js["time"] << "[" << js["id"] << "]" << js["name"] << " said: " << js["msg"] << endl;
                                    continue;
                                }
                                else if(GROUP_CHAT_MSG == msgtype){
                                    cout << "group msg[" << js["groupid"] << "]: " << js["time"] << "[" << js["id"] << "]" << js["name"] << " said: " << js["msg"] << endl;
                                    continue;
                                }
                            }
                        }
                        
                        // 登录成功，启用接收线程负责接收数据
                        std::thread readTask(readTaskHandler, clientfd);
                        readTask.detach();

                        // 进入聊天主菜单页面
                        isMainMenuRunning = true;
                        mainMenu(clientfd);
                    }
                }
            }
        }
        case 3: // quit
            close(clientfd);
            exit(0);
        default:
            cerr << "invalid input!";
            break;
        }
    }

    return 0;
}

// 接收线程
void readTaskHandler(int clientfd){
    for(;;){
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if(-1 == len || 0 == len){
            close(clientfd);
        }
        
        // 接收ChatServer转发的数据，反序列化生成json对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if(ONE_CHAT_MSG == msgtype){
            cout << js["time"] << "[" << js["id"] << "]" << js["name"] << " said: " << js["msg"] << endl;
            continue;
        }
        else if(GROUP_CHAT_MSG == msgtype){
            cout << "group msg[" << js["groupid"] << "]: " << js["time"] << "[" << js["id"] << "]" << js["name"] << " said: " << js["msg"] << endl;
            continue;
        }
    }
}

// 获取时间（聊天信息需要添加时间信息）
string getCurrentTime(){
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d", 
        (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
        (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
        
    return std::string(date);
}

void help(int fd = 0, string str = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void loginout(int, string str = "");

// 系统支持的客户端命令列表
unordered_map<string,string> commandMap={
    {"help", "显示所有支持的命令, 格式help"},
    {"chat", "一对一聊天, 格式chat:friendid:message"},
    {"addfriend", "添加好友, 格式addfriend:friendid"},
    {"creategroup", "创建群组, 格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组, 格式addgroup:groupid"},
    {"groupchat", "群聊, 格式groupchat:groupid:message"},
    {"loginout", "注销, 格式loginout"}
};

// 系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}
};

// 主聊天页面程序
void mainMenu(int clientfd){
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning){
        cin.getline(buffer, 1024);
        string commandbuffer(buffer);
        string command;
        int idx = commandbuffer.find(":");
        if(-1 == idx){
            command = commandbuffer;
        }
        else{
            command = commandbuffer.substr(0, idx);
        }

        auto it = commandHandlerMap.find(command);
        if(it == commandHandlerMap.end()){
            cerr << "invalid input command!" << endl;
            continue;
        }

        // 调用相应事件处理回调
        it->second(clientfd, commandbuffer.substr(idx+1, commandbuffer.size() - idx));
    }
}

void help(int, string){
    cout << "show command list >>>" << endl;
    for(auto &p : commandMap){
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

void addfriend(int clientfd, string str){
    int friendid = stoi(str);
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len){
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}

void chat(int clientfd, string str){
    int idx = str.find(":");
    if(-1 == idx){
        cerr << "chat command invalid!" << endl;
        return;
    }

    int friendid = stoi(str.substr(0, idx));
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["msg"] = message;
    js["to"] = friendid;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len){
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

// creategroup:groupname:groupdesc
void creategroup(int clientfd, string str){
    int idx = str.find(":");
    if(-1 == idx){
        cerr << "creategroup command invalid!" << endl;
        return;
    }

    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;

    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len){
        cerr << "send create group msg error -> " << buffer << endl;
    }
}

// addgroup:groupid:message
void addgroup(int clientfd, string str){
    int groupid = stoi(str);
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len){
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
}

// groupchat:groupid:message
void groupchat(int clientfd, string str){
    int idx = str.find(":");
    if(-1 == idx){
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    int groupid = stoi(str.substr(0, idx));
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len){
        cerr << "send group chat msg error -> " << buffer << endl;
    }
}

void loginout(int clientfd, string){
    json js;
    js["msgid"] = LOGIN_OUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str())+1, 0);
    if(-1 == len){
        cerr << "send login out msg error -> " << buffer << endl;
    }
    else{
        isMainMenuRunning = false;
    }

    g_currUserFriendList.clear();
    g_currUserGroupList.clear();
}

void showCurrentUserData()
{
    cout << "====================login user====================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "--------------------friend list--------------------" << endl;
    if (!g_currUserFriendList.empty())
    {
        for (User &user : g_currUserFriendList)
        {
            cout << user.getId() << "\t" << user.getName() << "\t" << user.getState() << endl;
        }
    }
    cout << "--------------------group list--------------------" << endl;
    if (!g_currUserGroupList.empty())
    {
        for (Group &group : g_currUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState() << " " << user.getRole() << endl;
            }
        }
    }
    cout << "==================================================" << endl;
}