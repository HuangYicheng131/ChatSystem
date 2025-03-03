#include "groupmodel.hpp"
#include "db.hpp"

// 创建群组
bool GroupModel::createGroup(Group &group){
    string sql = "insert into AllGroup(groupname, groupdesc) values('" + group.getName() + "', '" + group.getDesc() + "')";

    MySQL mysql;
    if (mysql.connect())
    {
        if(mysql.update(sql)){
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// 加入群组
void GroupModel::addGroup(int userid, int groupid, string role){
    string sql = "insert into GroupUser values(" + to_string(groupid) + ", " + to_string(userid) +", '" + role + "')";
    
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid){
    // 1. 先根据 userid 在 groupuser 表中查询出该用户所属的群组信息
    // 2. 再根据群组信息，查询属于该群组的所有用户的userid，并且和user表进行多表联合查询，查出用户的详细信息
    string sql = "select a.id, a.groupname, a.groupdesc from AllGroup a inner join \
        GroupUser b on a.id = b.groupid where b.userid = " + to_string(userid);
    
    vector<Group> groupVec;

    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            // 查出userid所有的群组信息
            while((row = mysql_fetch_row(res)) != nullptr){
                Group group;
                group.setId(stoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

    // 查询群组的用户信息
    for(Group &group: groupVec){
        sql = "select a.id, a.name, a.state, b.grouprole from User a inner join \
            GroupUser b on a.id = b.userid where b.groupid = " + to_string(group.getId());

        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            // 查出群组 group 中所有组员的信息
            while((row = mysql_fetch_row(res)) != nullptr){
                GroupUser user;
                user.setId(stoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                group.getUsers().push_back(user);
            }
            mysql_free_result(res);
        }
    }

    return groupVec;
}

// 根据指定的userid查询群组用户id列表，除userid自己，主要用于群聊业务给其他组员群发消息
vector<int> GroupModel::queryGroupsUsers(int userid, int groupid){
    string sql = "select userid from GroupUser where groupid = " + to_string(groupid) + " and userid != " + to_string(userid);

    vector<int> idVec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES* res = mysql.query(sql);
        if(res != nullptr){
            MYSQL_ROW row;
            // 查出群组中除userid外的所有成员id
            while((row = mysql_fetch_row(res)) != nullptr){
               idVec.push_back(stoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return idVec;
}