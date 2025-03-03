#ifndef GROUP_H
#define GROUP_H

#include "groupuser.hpp"
#include <string>
#include <vector>
using namespace std;

class Group{
public:
    Group(int id = -1, string name = "", string desc = ""){
        _id = id;
        _name = name;
        _desc = desc;
    }

    void setId(int id) { _id = id; }
    void setName(string name) { _name = name; }
    void setDesc(string desc) { _desc = desc; }

    int getId() { return _id; }
    string getName() { return _name; }
    string getDesc() { return _desc; }
    vector<GroupUser>& getUsers() { return users; } // 返回该群组的成员

private:
    int _id;
    string _name;
    string _desc;
    vector<GroupUser> users;
};

#endif