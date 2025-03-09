#ifndef SQL_CHAT_SESSION_MEMBER_HPP
#define SQL_CHAT_SESSION_MEMBER_HPP

#include "sql_factory.hpp"
#include "chat_session_member.hxx"
#include "chat_session_member-odb.hxx"

#include "logger.hpp"

namespace chat_im::util{

class ChatSessionMemberTable{
public:
    using ptr = std::shared_ptr<ChatSessionMemberTable>;
    using query = odb::query<chatSessionMember>;
    using result = odb::result<chatSessionMember>;

    ChatSessionMemberTable(const std::shared_ptr<odb::core::database>& db):__db(db){}

    bool insert(chatSessionMember& member){
        try
        {
            odb::transaction trans(__db->begin());
            __db->persist(member);
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("聊天会话成员插入失败:{}",e.what());
            return false;
        }
        return true;
    }
    bool insert(const std::shared_ptr<chatSessionMember> &member)
    {
        return insert(*member);
    }
    bool insert(const std::string& chat_session_id,std::vector<std::string> user_id_list){
        try
        {
            odb::transaction trans(__db->begin());
            for(const std::string&id : user_id_list){
                chatSessionMember member(chat_session_id,id);
                __db->persist(member);
            }
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("聊天会话成员插入失败:{}",e.what());
            return false;
        }
        return true;
    }

    bool remove(const std::string &chat_session_id, const std::string &user_id)
    {
        try
        {
            odb::transaction trans(__db->begin());
            __db->erase_query<chatSessionMember>(
                query::chat_session_id == chat_session_id &&
                query::user_id == user_id);
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("删除{}-{}聊天会话成员失败",chat_session_id,user_id);
            return false;
        }
        return true;
    }

    std::vector<chatSessionMember> select_chat_session_id(const std::string &id)
    {
        std::vector<chatSessionMember> ret;
        try
        {
            odb::transaction trans(__db->begin());
            result res(__db->query<chatSessionMember>(query::chat_session_id == id));
            ret.reserve(res.size());
            for(auto i = res.begin();i!=res.end();++i){
                ret.push_back(*i);
            }
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("查询聊天会话{}成员失败:{}",id,e.what());
        }
        return ret;
    }

    std::vector<chatSessionMember> select_user_id(const std::string &id)
    {
        std::vector<chatSessionMember> ret;
        try
        {
            odb::transaction trans(__db->begin());
            result res(__db->query<chatSessionMember>(query::user_id == id));
            ret.reserve(res.size());
            for(auto i = res.begin();i!=res.end();++i){
                ret.push_back(*i);
            }
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("查询{}的聊天会话失败:{}",id,e.what());
        }
        return ret;
    }


private:
    std::shared_ptr<odb::core::database> __db;

};


}



#endif 