#ifndef CHAT_SESSION_NUMBER_HXX
#define CHAT_SESSION_NUMBER_HXX

#include <odb/core.hxx>
#include <odb/nullable.hxx>
#include <string>

namespace chat_im{


#pragma db object table("chatSessionMember")
class chatSessionMember{
public:
    chatSessionMember() {}
    ~chatSessionMember() {}

    chatSessionMember(
        const std::string& chat_session_id,
        const std::string& user_id 
    ):__chat_session_id(chat_session_id),__user_id(user_id){}

    void chat_session_id(const std::string& id){__chat_session_id = id;}
    std::string chat_session_id(){return __chat_session_id;}

    void user_id(const std::string& id){__user_id = id;}
    std::string user_id(){return __user_id;}

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long __id;

    #pragma db index type("varchar(256)")
    std::string __chat_session_id;

    #pragma db index type("varchar(256)")
    std::string __user_id;


};

}



#endif 