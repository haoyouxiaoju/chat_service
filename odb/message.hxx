#ifndef MESSAGE_XIAOJU_HPP
#define MESSAGE_XIAOJU_HPP

#include <odb/core.hxx>
#include <odb/nullable.hxx>

#include <boost/date_time/posix_time/posix_time.hpp>


//odb -d mysql --generate-query --generate-schema --profile boost/date-time message.hxx
namespace chat_im
{
#pragma db object table("Message")
class Message_ODB{
public:
    Message_ODB(
        const std::string& sender_id,
        const std::string& messageSession_id,
        const boost::posix_time::ptime& create_time,
        signed char message_type
    ):__sender_id(sender_id),__messageSession_id(messageSession_id),
        __create_time(create_time),__message_type(message_type){}
    Message_ODB(
        const std::string &sender_id,
        const std::string &messageSession_id,
        const std::string& message_id,
        const std::string& message_content
    ) 
        : __sender_id(sender_id), __messageSession_id(messageSession_id),
         __message_id(message_id),__message_text(message_content) {}
    
    Message_ODB(){}    
    ~Message_ODB(){}

    void create_time(const boost::posix_time::ptime& time){__create_time = time;}
    boost::posix_time::ptime create_time(){return __create_time;}

    void message_id(const std::string& id){__message_id = id;}
    std::string message_id(){return __message_id;}
    
    void sender_id(const std::string& sender_id){__sender_id = sender_id;}
    std::string sender_id(){return __sender_id;}

    void messageSession_id(const std::string& messageSession_id){__messageSession_id = messageSession_id;}
    std::string messageSession_id(){return __messageSession_id;}

    void message_type(signed char type){__message_type = type;}
    signed char message_type(){return __message_type;}

    void message_text(const std::string& content){__message_text=content;}
    std::string message_text(){if(!__message_text)return std::string();     return *__message_text;}

    void file_id(const std::string& file_id){__file_id = file_id;}
    std::string file_id(){if(!__file_id)return std::string();                return *__file_id;}

    void file_size(const unsigned long file_size){__file_size = file_size;}
    unsigned long file_size(){if(!__file_size)return 0;                      return *__file_size;}

    void file_name(const std::string& file_name){__file_name = file_name;}
    std::string file_name(){if(!__file_name)return std::string();            return *__file_name;}

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long __id;

    #pragma db type("VARCHAR(128)") unique index
    std::string __message_id;

    #pragma db type("TIMESTAMP") not_null
    boost::posix_time::ptime  __create_time;    //消息的发送时间

    #pragma db type("VARCHAR(128)") not_null index
    std::string __sender_id;                    //发送者的用户id

    #pragma db type("VARCHAR(128)") not_null index
    std::string __messageSession_id;            //消息所属的会话id

    signed char __message_type;                 //消息类型 0-文本 1-图片 2-文件 3-语音  //与pb文件对齐

    odb::nullable<std::string> __message_text;  //文本消息内容

    #pragma db type("VARCHAR(128)") 
    odb::nullable<std::string> __file_id;       //文件id

    odb::nullable<unsigned long> __file_size;   //文件大小

    #pragma db type("VARCHAR(128)")
    odb::nullable<std::string> __file_name;     //文件名称

};
    
} // namespace chat_im

#endif