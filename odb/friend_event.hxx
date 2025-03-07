#ifndef FRIEND_EVENT_HXX
#define FRIEND_EVENT_HXX
#include <odb/core.hxx>
#include <odb/nullable.hxx>

#include <string>

namespace chat_im{

enum class friendStatus{
    PENDING = 1,
    ACCEPT = 2,
    REJECT =3
};

#pragma db object table("friendEvent")
class friendEvent{

public:
    friendEvent(
        const std::string& event_id,
        const std::string& sender_id,
        const std::string& reciver_id,
        friendStatus status = friendStatus::PENDING
    ):__sender_id(sender_id),__receiver_id(reciver_id){}

    void friend_event_id(const std::string& event_id){__friend_event_id = event_id;}
    std::string friend_event_id(){return __friend_event_id;}

    void sender_id(const std::string& user_id){__sender_id = user_id;}
    std::string sender_id(){return __sender_id;}

    void receiver_id(const std::string& user_id){__receiver_id = user_id;}
    std::string reveiver_id(){return __receiver_id;}

    void event_status(friendStatus status){__event_status = status;}
    friendStatus  event_status(){return __event_status;}


private:
    friend class odb::access;

    #pragma db id auto
    unsigned long __id;

    #pragma db type("VARCHAR(256)") unique
    std::string __friend_event_id;

    #pragma db type("VARCHAR(256)") index
    std::string __sender_id;

    #pragma db type("VARCHAR(256)") index
    std::string __receiver_id;

    #pragma db type("TINYINT")
    friendStatus __event_status;




};


}



#endif