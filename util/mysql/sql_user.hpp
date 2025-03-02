#ifndef SQL_USER_HPP
#define SQL_USER_HPP

#include "sql_factory.hpp"
#include "logger.hpp"
#include "user.hxx"
#include "user-odb.hxx"

namespace chat_im::util{


//
// mysql数据库中的user表操作类
//      插入新用户 
//      修改用户
//      用户昵称/手机号码/用户id来查询用户


class UserTable{
public:
    using ptr = std::shared_ptr<UserTable>;
    using qurey = odb::query<User>; 
    using result = odb::result<User>;

    UserTable(const std::shared_ptr<odb::core::database>& db):
        __db(db){}

    bool insert(User& user){
        try{
            odb::transaction trans(__db->begin());
            __db->persist(user);
            trans.commit();
            
        }catch(std::exception& e){
            ERROR("添加用户失败:{}",e.what());
            return false;
        }
        return true;
    }
    bool insert(const std::shared_ptr<User>& user){
        return this->insert(*user);
    }

    bool update(User& user){
        try{
            odb::transaction trans(__db->begin());
            __db->update(user);
            trans.commit();

        }catch(std::exception& e){
            ERROR("修改用户失败:{}",e.what());
            
            return false;
        }

        return true;
    }
    bool update(const std::shared_ptr<User>& user){
        return this->update(*user);
    }

    //
    //  用户昵称/手机号码/用户id来查询用户
    //
    std::shared_ptr<User> select_by_nickname(const std::string& nickname){
        std::shared_ptr<User> ret;
        try{
            odb::transaction trans(__db->begin());
            ret.reset(__db->query_one<User>(qurey::nickname == nickname));
            trans.commit();

        }catch(std::exception& e){
            ERROR("查询{}用户名失败:{}",nickname,e.what());
        }
        return ret;
    }
    std::shared_ptr<User> select_by_phone(const std::string& phone){
        std::shared_ptr<User> ret;
        try{
            odb::transaction trans(__db->begin());
            ret.reset(__db->query_one<User>(qurey::phone == phone));
            trans.commit();

        }catch(std::exception& e){
            ERROR("查询{}用户手机号失败:{}",phone,e.what());
        }
        return ret;

    }
    std::shared_ptr<User> select_by_userId(const std::string& user_id){
        std::shared_ptr<User> ret;
        try{
            odb::transaction trans(__db->begin());
            ret.reset(__db->query_one<User>(qurey::user_id == user_id));
            trans.commit();

        }catch(std::exception& e){
            ERROR("查询{}用户id失败:{}",user_id,e.what());
        }
        return ret;

    }
    
    std::vector<User>  select_multi_users(const std::vector<std::string>& id_list){
        if(id_list.empty()){
            return std::vector<User>();
        }
        std::vector<User> ret;
        try{
            //拼接查询命令 user_id in (id1,id2,id3,...)
            
            odb::transaction trans(__db->begin());
            std::stringstream ss;
            ss<< "user_id in (" ;
            size_t size = id_list.size();
            for(int i=0;i<size-1 ;++i){
                ss<<"'"<<id_list[i]<<"',";
            }
            ss<<"'"<<id_list[size-1]<<"')";
            result res(__db->query<User>(ss.str()));
            for (result::iterator i(res.begin()); i != res.end(); ++i) {
                ret.push_back(*i);
            } 
            trans.commit();
            // for(const auto &e : res ){
            //     ;
            //     ret.push_back(e);
            // }
            
            
        }catch(std::exception& e){
            ERROR("用户id批量查询失败:{}",e.what());
        }
        return ret;
    }

private:
    std::shared_ptr<odb::core::database> __db;


};



}




#endif