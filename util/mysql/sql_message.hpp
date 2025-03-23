#ifndef SQL_MESSAGE_HPP
#define SQL_MESSGAE_HPP


#include "sql_factory.hpp"
#include "message.hxx"
#include "message-odb.hxx"

#include "logger.hpp"


namespace chat_im::util{

class MessageTable{

public:

    using ptr = std::shared_ptr<MessageTable>;
    using query = odb::query<Message_ODB>;
    using result = odb::result<Message_ODB>;

    MessageTable(const std::shared_ptr<odb::core::database>& db):__db(db){}
    
    //插入消息
    bool insert(Message_ODB& message){
        try{
            odb::transaction trans(__db->begin());
           __db->persist(message); 
            trans.commit();
        }catch(std::exception& e){
            ERROR("消息信息插入失败原因{}:,发送者{}-发送类型{}-发送时间{}",e.what(),message.sender_id(),message.message_type(), boost::posix_time::to_simple_string( message.create_time()));
            
            return false;
        }
        return true;
    }
    //插入消息
    bool insert(const std::shared_ptr<Message_ODB>& message){
        return this->insert(*message);
    }

    //删除指定会话的所有消息
    bool remove(const std::string& session_id){
        try{
            odb::transaction trans(__db->begin());
            __db->erase_query<Message_ODB>(query::messageSession_id == session_id);
            trans.commit();
        }catch(std::exception& e){
            ERROR("删除{}会话所有消息失败:{}",session_id,e.what());
            return false;
        }
        return true;
    }

    //通过消息id来查询指定的消息
    std::shared_ptr<Message_ODB> select_by_messageId(const std::string& id){
        std::shared_ptr<Message_ODB> ret;
        try{
            odb::transaction trans(__db->begin());
            ret.reset(__db->query_one<Message_ODB>(query::message_id == id));
            trans.commit();
        }catch(std::exception& e){
            ERROR("查询消息id失败:{}",e.what());
        }
        return ret;
    }

    //根据时间查询指定时间段内,消息会话的信息
    //  --- begin_time > end_time 
    std::vector<Message_ODB> select_by_time(
        const std::string& session_id,
        const boost::posix_time::ptime& begin_time,
        const boost::posix_time::ptime& end_time){
        std::vector<Message_ODB> ret;
        try{
            odb::transaction trans(__db->begin());
            result res(__db->query<Message_ODB>(
                query::messageSession_id == session_id &&
                query::create_time <= end_time   &&
                query::create_time >= begin_time
            ));
            for(auto i = res.begin();i!=res.end();++i){
                ret.push_back(*i);
            }
            trans.commit();            
        }catch(std::exception& e){
            ERROR("{}根据时间查询消息失败:{}",session_id,e.what());
        }
        return ret;
    }

    //查询消息会话最近的几条消息
    std::vector<Message_ODB> select_numberMessage(
        const std::string& session_id,
        int number
    ){
        std::vector<Message_ODB> ret;
        try{
            odb::transaction trans(__db->begin());
            std::stringstream ss;
            ss  << "messageSession_id='"
                << session_id
                << "' order by create_time desc limit "
                << number;
            result res(__db->query<Message_ODB>(ss.str()));
            for(auto i = res.begin();i!=res.end();++i){
                ret.push_back(*i);
            }
            trans.commit();
        }catch(std::exception& e){
            ERROR("{}查询最近消息失败:{}",session_id,e.what());
        }
        return ret;
    }



private:
    std::shared_ptr<odb::core::database> __db;


};


}




#endif