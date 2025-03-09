#ifndef SQL_CHAT_SESSION_HPP
#define SQL_CHAT_SESSION_HPP
#include "sql_factory.hpp"
#include "chat_session.hxx"
#include "chat_session-odb.hxx"

#include "logger.hpp"

namespace chat_im::util{

class ChatSessionTable{
public:
    using ptr = std::shared_ptr<ChatSessionTable>;
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

    std::vector<SingleChatSession> singleChatSession(const std::string &uid) {
        using query = odb::query<SingleChatSession>;
        using result = odb::result<SingleChatSession>;
        std::vector<SingleChatSession> res;
        try {
            odb::transaction trans(__db->begin());
            //当前的uid是被申请者的用户ID
            result r(__db->query<SingleChatSession>(
                query::css::chat_session_type == chatSessionType::SINGLE && 
                query::csm1::user_id == uid && 
                query::csm2::user_id != query::csm1::user_id));
            for (result::iterator i(r.begin()); i != r.end(); ++i) {
                res.push_back(*i);
            }
            trans.commit();
        }catch (std::exception &e) {
            ERROR("获取用户 {} 的单聊会话失败:{}！", uid, e.what());
        }
        return res;
    }
    std::vector<GroupChatSession> groupChatSession(const std::string &uid) {
        using query = odb::query<GroupChatSession>;
        using result = odb::result<GroupChatSession>;
        std::vector<GroupChatSession> res;
        try {
            odb::transaction trans(__db->begin());
           
            //当前的uid是被申请者的用户ID
            result r(__db->query<GroupChatSession>(
                query::css::chat_session_type == chatSessionType::GROUP && 
                query::csm::user_id == uid ));
            for (result::iterator i(r.begin()); i != r.end(); ++i) {
                res.push_back(*i);
            }
            trans.commit();
        }catch (std::exception &e) {
            ERROR("获取用户 {} 的群聊会话失败:{}！", uid, e.what());
        }
        return res;
    }

    std::shared_ptr<chatSession> select_chat_session_id(const std::string& id ){
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

    std::vector<chatSession> select_chat_session_id_list(const std::vector<std::string> &id_list)
    {
        std::vector<chatSession> ret;
        try
        {
            odb::transaction trans(__db->begin());
            std::stringstream ss;
            ss<< "chat_session_id in (" ;
            size_t size = id_list.size();
            for(int i=0;i<size-1 ;++i){
                ss<<"'"<<id_list[i]<<"',";
            }
            ss<<"'"<<id_list[size-1]<<"')";
            result res(__db->query<chatSession>(ss.str()));
            for (result::iterator i(res.begin()); i != res.end(); ++i) {
                ret.push_back(*i);
            } 
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("查询聊天会话失败:{}", e.what());
        }
        return ret;
    }

private:
    std::shared_ptr<odb::core::database> __db;    

};



}



#endif