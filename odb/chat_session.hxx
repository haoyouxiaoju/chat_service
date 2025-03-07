#ifndef CHAT_SESSION_HXX
#define CHAT_SESSION_HXX

#include <odb/core.hxx>
#include <odb/nullable.hxx>

#include <string>

#include "chat_session_number.hxx"

namespace chat_im{

enum class chatSessionType{
    SINGLE = 1,
    GROUP = 2
};


#pragma db object table("chatSession")
class chatSession{
public:
    chatSession(){}
    chatSession(
        const std::string& chat_session_id,
        const std::string& chat_session_name,
        chatSessionType chat_type 
    ):__chat_session_id(chat_session_id),__chat_session_name(chat_session_name),
        __chat_session_type(chat_type){}


    void chat_session_id(const std::string& id){__chat_session_id = id;}
    std::string chat_session_id(){return __chat_session_id;}

    void chat_session_name(const std::string& name){__chat_session_name = name;}
    std::string chat_session_name(){return *__chat_session_name;}

    void chat_session_type(chatSessionType type){__chat_session_type = type;}
    chatSessionType chat_session_type(){return __chat_session_type;}



private:
    friend class odb::access;

    #pragma db id auto
    unsigned long __id;

    #pragma db unique type("VARCHAR(256)")
    std::string __chat_session_id;

    #pragma db type("VARCHAR(256)")
    odb::nullable<std::string> __chat_session_name;

    #pragma db type("TINYINT")
    chatSessionType __chat_session_type;

};

// 这里条件必须是指定条件：  css::chat_session_type==1 && csm1.user_id=uid && csm2.user_id != csm1.user_id
#pragma db view object(chatSession = css)\
                object(chatSessionMember = csm1 : css::__chat_session_id == csm1::__chat_session_id)\
                object(chatSessionMember = csm2 : css::__chat_session_id == csm2::__chat_session_id)\
                query((?))
struct SingleChatSession {
    #pragma db column(css::__chat_session_id)
    std::string chat_session_id;
    #pragma db column(csm2::__user_id)
    std::string friend_id;
};

// 这里条件必须是指定条件：  css::chat_session_type==2 && csm.user_id=uid
#pragma db view object(chatSession = css)\
                object(chatSessionMember = csm : css::__chat_session_id == csm::__chat_session_id)\
                query((?))
struct GroupChatSession {
    #pragma db column(css::__chat_session_id)
    std::string chat_session_id;
    #pragma db column(css::__chat_session_name)
    std::string chat_session_name;
};

}

#endif