#ifndef SQL_FRIEND_RALATION_HPP
#define SQL_FRIEND_RALATION_HPP

#include "sql_factory.hpp"
#include "friend_relation.hxx"
#include "friend_relation-odb.hxx"

#include "logger.hpp"


namespace chat_im::util{

class FriendRalationTable{
public:
    using ptr = std::shared_ptr<friendRalation>;
    using query = odb::query<friendRalation>;
    FriendRalationTable(const std::shared_ptr<odb::core::database>& db):__db(db){}
    
    bool insert(friendRalation& elem){
        try{
            odb::transaction trans(__db->begin());
            __db->persist(elem);
            //需要插入第二次 即 好友对 用户的好友关系
            __db->persist(FriendRalation(elem.friend_id(),elem.user_id()));
            trans.commit();
        }catch(std::exception& e){
            ERROR("插入{}-{}好友关系记录失败:{}",elem.user_id(),elem.friend_id(),e.what());
            return false;
        }
        return true;
    }

    bool insert(const std::shared_ptr<friendRalation>& elem){
        return insert(*elem);
    }

    bool remove(const std::string& user_id,const std::string& friend_id){
        try{
            odb::transaction trans(__db->begin());
            __db->erase_query<friendRalation>(
                (query::user_id == user_id && query::friend_id == friend_id)
                (query::user_id == friend_id && query::friend_id == user_id)
            );
            trans.commit();
        }catch(std::exception& e){
            ERROR("删除{}-{}的好友关系失败:{}",user_id,friend_id,e.what());
            return false;
        }
        return true;

    }

    //  判断是否为好友 true -- 为好友 
    bool isFriend(const std::string& user_id,const std::string& friend_id){
        bool flags = false;
        try{
            odb::transaction trans(__db->begin());
            result res(__db->query<friendRalation>(query::user_id == user_id && query::friend_id == friend_id));
            flags = !res.empty();
            trans.commit();
        }catch(std::exception& e){
            ERROR("获取用户好友关系失败:{}-{}-{}!",user_id,friend_id,e.what());
        }
        return flags;

    }

    std::vector<std::string> select_friend(const std::string& user_id){
        std::vector<std::string> ret;
        try{
            odb::transaction trans(__db->begin());
            result res(__db->query<friendRalation>(query::user_id == user_id));
            ret.reserve(res.size());
            for(auto i=res.begin();i!=res.end();++i){
                ret.push_back((*i).friend_id());
            }
            trans.commit();
        }catch(std::exception& e){
            ERROR("获取{}的好友列表失败:{}",user_id,e.what());
        }
        return ret;
    }

private:
    std::shared_ptr<odb::core::database> __db;
};





}




#endif