#include "friendmodel.hpp"
#include "db.hpp"

// 添加好友关系
void FriendModel::insert(int userid, int friendid){
    string sql = "insert into Friend values(" + to_string(userid) + ", " + to_string(friendid) + ")";

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 返回用户好友列表 friendid
vector<User> FriendModel::query(int userid){
    string sql = "select a.id, a.name, a.state from User a inner join Friend b on b.friendid=a.id where b.userid=" + to_string(userid);

    MySQL mysql;
    vector<User> vec;
    if (mysql.connect())
    {
        // 把用户 userid 的所有好友 friendid 放入 vec 中
        MYSQL_RES*res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while(row = mysql_fetch_row(res)){
                User user;
                user.setId(stoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
        }
        mysql_free_result(res);
    }

    return vec;
}