#ifndef SQL_FRIEND_EVENT_HPP
#define SQL_FRIEND_EVENT_HPP

#include "sql_factory.hpp"
#include "friend_event.hxx"
#include "friend_event-odb.hxx"

#include "logger.hpp"

namespace chat_im::util{

class FriendEventTable{
public:
    using ptr = std::shared_ptr<FriendEventTable>;
    using query = odb::query<friendEvent>;
    using result = odb::result<friendEvent>;

    FriendEventTable(const std::shared_ptr<odb::core::database>& db):__db(db){}

    bool insert(friendEvent& event){
        try{
            odb::transaction trans(__db->begin());
            __db->persist(event);
            trans.commit();
        }catch(std::exception& e){
            ERROR("好友申请事件插入失败:{}",e.what());
            return false;
        }
        return true;

    }
    bool insert(std::shared_ptr<friendEvent>& event){
        return insert(*event);
    }

    bool remove(const std::string& id){
        try{
            odb::transaction trans(__db->begin());
            __db->erase_query<friendEvent>(query::friend_event_id == id);
            trans.commit();
        }catch(std::exception& e){
            ERROR("好友申请事件删除失败:{}",e.what());
            return false;
        }
        return true;
    }

    bool update(friendEvent& event){
        try{
            odb::transaction trans(__db->begin());
            __db->update(event);
            trans.commit();
        }catch(std::exception& e){
            ERROR("修改好友申请记录失败:{}",e.what());
            return false;
        }
        return true;
    }
    
    bool update(std::shared_ptr<friendEvent>& event){
        return update(*event);
    }

 

    std::vector<friendEvent> select_senderId(const std::string& id){
        std::vector<friendEvent> ret;
        try{
            odb::transaction trans(__db->begin());
            result res(__db->query<friendEvent>(query::sender_id == id));
            ret.reserve(res.size());
            for(auto i=res.begin();i!=res.end();++i){
                ret.push_back(*i);
            }
            trans.commit();
        }catch(std::exception& e){
            ERROR("获取{}的好友申请记录失败:{}",id,e.what());
        }
        return ret;

    }

    std::vector<friendEvent> select_receiverId(const std::string& id){
        std::vector<friendEvent> ret;
        try{
            odb::transaction trans(__db->begin());
            result res(__db->query<friendEvent>(query::receiver_id == id));
            ret.reserve(res.size());
            for(auto i=res.begin();i!=res.end();++i){
                ret.push_back(*i);
            }
            trans.commit();
        }catch(std::exception& e){
            ERROR("获取{}的被好友申请记录失败:{}",id,e.what());
        }
        return ret;

    }

    std::shared_ptr<friendEvent> select_eventId(const std::string& id){
        std::shared_ptr<friendEvent> ret;
        try{
            odb::transaction trans(__db->begin());
            ret.reset(__db->query_one<friendEvent>(query::friend_event_id == id));
            trans.commit();
        }catch(std::exception& e){
            ERROR("获取{}好友申请记录失败:{}",id,e.what());
        }
        return ret;

    }

    std::shared_ptr<friendEvent> select_userIdAndFriendId(const std::string& user_id,const std::string& friend_id){
        std::shared_ptr<friendEvent> ret;
        try{
            odb::transaction trans(__db->begin());
            ret.reset(__db->query_one<friendEvent>(query::sender_id == user_id && query::receiver_id == friend_id));
            if(!ret){
                ret.reset(__db->query_one<friendEvent>(query::sender_id == friend_id && query::receiver_id == user_id));
            }
            trans.commit();
        }catch(std::exception& e){
            ERROR("获取{}好友申请记录失败:{}",e.what());
        }
        return ret;

    }



private:
    std::shared_ptr<odb::core::database> __db;

};




}





#endif