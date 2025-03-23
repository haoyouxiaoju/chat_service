#ifndef DATA_REDIS_HPP
#define DATA_REDIS_HPP

#include <sw/redis++/redis.h>
#include <iostream>

namespace chat_im
{

namespace util{

class RedisClientFactory{

public:
    static std::shared_ptr<sw::redis::Redis> create(
        const std::string& host,
        int port,
        int db,
        bool keep_alive
    ){
        sw::redis::ConnectionOptions opts;
        opts.host = host;
        opts.port = port;
        opts.db = db;
        opts.keep_alive = keep_alive;
        return std::make_shared<sw::redis::Redis>(opts);
    }
};


//  redis中会话id <=> uid
//  向redis中   添加pair(会话id，用户id) 
//              删除会话id对应的pair
//              获取会话id对应的用户id
class Session{
public:
    using ptr = std::shared_ptr<Session>;
    Session(const std::shared_ptr<sw::redis::Redis>& redis_client):__redis_client(redis_client){}

    void append(const std::string& sid,const std::string& uid){
        __redis_client->set(sid,uid,std::chrono::milliseconds(6000000));
    }
    void remove(const std::string& sid){
        __redis_client->del(sid);
    }
    sw::redis::OptionalString uid(const std::string& sid){
        return __redis_client->get(sid);
    }

private:
    std::shared_ptr<sw::redis::Redis> __redis_client;
};


//  redis 中 会话id <=> status
//      添加登录状态 pair(会话id,status)
//      删除登录状态 会话id
//      获取登录状态
class Status{
public:
    using ptr = std::shared_ptr<Status>;
    Status(const std::shared_ptr<sw::redis::Redis>& redis_client):__redis_client(redis_client){}

    void append(const std::string& uid){
        __redis_client->set(uid," ");
    }

    void remove(const std::string& uid){
        __redis_client->del(uid);
    }

    //true - 已登录  , false - 不存在
    bool isExist(const std::string& uid){
        sw::redis::OptionalString val = __redis_client->get(uid);
        if(val)
            return true;
        return false;
    }

private:
    std::shared_ptr<sw::redis::Redis> __redis_client;

};


//  redis 中 code id <=> code
//      添加验证码(code_id,code)
//      获取验证码code
//
class Code{
public:
    using ptr = std::shared_ptr<Code>;
    Code(const std::shared_ptr<sw::redis::Redis> redis_client):__redis_client(redis_client){}

    void append(const std::string& cid,const std::string& code){
        __redis_client->set(cid,code);
    }

    sw::redis::OptionalString code(const std::string& cid){
        return __redis_client->get(cid);
    }


private:
    std::shared_ptr<sw::redis::Redis> __redis_client;
};

}



    
} // namespace chat_im






#endif