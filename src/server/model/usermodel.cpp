#include "usermodel.hpp"
#include "db.hpp"

// User表插入
bool UserModel::insert(User &user)
{
    // 1. 组装sql语句
    string sql = "insert into User(name, password, state) values('" + user.getName() + "', '" + user.getPassword() + "', '" + user.getState() + "')";

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取插入成功的用户数据生成的id
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// 根据id查询用户信息
User UserModel::query(int id)
{
    // 1. 组装sql语句
    string sql = "select * from User where id=" + to_string(id);

    MySQL mysql;

    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr)
            {
                User user;
                user.setId(stoi(row[0]));
                user.setName(row[1]);
                user.setPassword(row[2]);
                user.setState(row[3]);

                return user;
            }
        }
    }

    return User();
}

// 更新用户状态信息
bool UserModel::updateState(User &user)
{
    // 1. 组装sql语句
    string sql = "update User set state='" + user.getState() + "' where id=" + to_string(user.getId());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            return true;
        }
    }

    return false;
}

// 重置用户状态信息
void UserModel::resetState()
{
    // 1. 组装sql语句
    string sql = "update User set state='offline' where state ='online'";

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}