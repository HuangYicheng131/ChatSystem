#include "offlinemessagemodel.hpp"
#include "db.hpp"

// 存储用户的离线消息
void OfflineMsgModel::insert(int id, string msg)
{
    string sql = "insert into OfflineMessage values(" + to_string(id) + ", '" + msg + "')";

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 删除用户的离线消息
void OfflineMsgModel::remove(int id)
{
    string sql = "delete from OfflineMessage where userid=" + to_string(id);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户的离线消息
vector<string> OfflineMsgModel::query(int id)
{
    string sql = "select message from OfflineMessage where userid=" + to_string(id);

    MySQL mysql;
    vector<string> vec;
    if (mysql.connect())
    {
        MYSQL_RES*res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            while(row = mysql_fetch_row(res)){
                vec.push_back(row[0]);
            }
        }

        mysql_free_result(res);
    }

    return vec;
}