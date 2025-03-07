#ifndef SQL_CHAT_SESSION_HPP
#define SQL_CHAT_SESSION_HPP
#include "sql_factory.hpp"
#include "chat_session.hxx"
#include "chat_session-odb.hxx"

#include "logger.hpp"

namespace chat_im::util{

class ChatSessionTable{
public:
    using ptr = std::shared_ptr<chatSession>;
    using query = odb::query<chatSession>;
    using result = odb::result<chatSession>;

    ChatSessionTable(const std::shared_ptr<odb::core::database>& db):__db(db){}

    bool insert(chatSession& session){
        try{
            odb::transaction trans(__db->begin());
            __db->persist(session);
            trans.commit();
        }catch(std::exception& e){
            ERROR("插入聊天会话失败:{}",e.what());
            return false;
        }
        return true;
    }
    bool insert(std::shared_ptr<chatSession>& session){
        return insert(*session);
    }

    bool remove(const std::string& chat_session_id){
        try{
            odb::transaction trans(__db->begin());
            __db->erase_query<chatSession>(query::chat_session_id == chat_session_id);
            trans.commit();
        }catch(std::exception& e){
            ERROR("删除{}聊天会话失败:{}",chat_session_id,e.what());
            return false;
        }
        return true;

    }

    bool update(chatSession& session){
        try{
            odb::transaction trans(__db->begin());
            __db->update(session);
            trans.commit();
        }catch(std::exception& e){
            ERROR("修改{}聊天会话失败:{}",session.chat_session_id(),e.what());
            return false;
        }
        return true;
    }



    std::shared_ptr<chatSession> select_chat_session_id(const std::string& id){
        std::shared_ptr<chatSession> ret;
        try{
            odb::transaction trans(__db->begin());
            ret.reset(__db->query_one<chatSession>(query::chat_session_id == id));
            trans.commit();
        }catch(std::exception& e){
            ERROR("查询{}聊天会话失败:{}",id,e.what());
        }
        return ret;
    }



private:
    std::shared_ptr<odb::core::database> __db;    

};



}



#endif