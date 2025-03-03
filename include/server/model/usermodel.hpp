#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

// User表数据操作类
class UserModel
{
public:
    // User表插入
    bool insert(User &user);
    
    // 根据id查询用户信息
    User query(int id);

    // 更新用户状态信息
    bool updateState(User& user);

    // 重置用户状态信息
    void resetState();
};

#endif