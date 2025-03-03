#include "json.hpp"
using json = nlohmann::json;

#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;

// example 1
string func1()
{
    json js;
    js["msg_type"] = 2;
    js["from"] = "Zhang San";
    js["to"] = "Li Si";
    js["msg"] = "Hello, what are you doing now ?";
    // cout << js << endl;

    string sendBuf = js.dump(); // 转成序列化json字符串
    // cout << sendBuf.c_str() << endl; 
    return sendBuf;
}

// 序列化容器
string func2(){
    json js;

    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(5);

    js["list"] = vec;

    map<int, string> mp;
    mp.insert({1, "黄山"});
    mp.insert({2, "泰山"});
    mp.insert({3, "华山"});

    js["path"] = mp;

    return js.dump();
}

// 反序列化

int main()
{
    string recvBuf = func1();
    json jsbuf = json::parse(recvBuf); // json字符串 -> json数据对象

    cout << jsbuf["msg_type"] << endl;

    return 0;
}