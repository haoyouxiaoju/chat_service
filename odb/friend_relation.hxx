#ifndef FRIEND_RALATION_HXX
#define FRIEND_RALATION_HXX

#include <odb/core.hxx>
#include <string>


namespace chat_im{

#pragma db object table("friendRelation")
class friendRelation{

public:
    friendRelation(const std::string& user_id,const std::string& friend_id):
        __user_id(user_id),__friend_id(friend_id){}
    
    void user_id(const std::string& id){__user_id = id;}
    std::string user_id(){return __user_id;}

    void friend_id(const std::string& id){__friend_id = id;}
    std::string friend_id(){return __friend_id;}

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long __id;

    #pragma db index type("VARCHAR(256)")
    std::string __user_id;

    #pragma db type("VARCHAR(256)")
    std::string __friend_id;

};



}




#endif