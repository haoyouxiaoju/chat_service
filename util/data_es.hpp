#ifndef DATA_ES_HPP
#define DATA_ES_HPP

#include "icsearch.hpp"
#include "logger.hpp"
#include "user.hxx"
#include "user-odb.hxx"

#include "message.hxx"
#include "message-odb.hxx"

#include <iostream>


namespace chat_im::util
{

class ESClientFactory{
public:
    static std::shared_ptr<elasticlient::Client> create(const std::vector<std::string> hosts){
        return std::make_shared<elasticlient::Client>(hosts);
    }

};



//
//  创建索引user
//  添加数据
//  
//  查询(多组)数据

class ESUser{
public:
    using ptr = std::shared_ptr<ESUser>;
    ESUser(const std::shared_ptr<elasticlient::Client>& es_client):__es_client(es_client){}

//  创建索引user
    bool createIndex(){
        bool ret = ESIndex(__es_client,"user")
            .append("nickname")
            .append("user_id","keyword","standard")
            .append("phone","keyword","standard")
            .append("signature","text","not_analyzed")
            .append("avatar_id","text","not_abalyzed")
            .create();
        if(!ret){
            ERROR("ES客户端 创建用户索引失败");
            return false;
        }

        return true;
    }

//  添加数据
    bool append(const std::shared_ptr<chat_im::User>& user){
        return append(user->nickname(),user->user_id(),user->phone(),user->signature(),user->avatar_id());
    }
    bool append(const std::string &nickname,
           const std::string &user_id,
           const std::string &phone,
           const std::string &signature,
           const std::string &avatar_id)
    {
        bool ret = ESInsert(__es_client,"user")
            .append("nickname",nickname)
            .append("user_id",user_id)
            .append("phone",phone)
            .append("signature",signature)
            .append("avatar_id",avatar_id)
            .insert(user_id);
        if(!ret){
            ERROR("插入用户 {} 数据成功",user_id);
            return false;
        }
        return true;
    }


//  查询(多组)数据
    std::vector<User> search(const std::string& key,const std::vector<std::string> uid_list){
        Json::Value users_json = ESSearch(__es_client,"user")
            .add_should_match("user_id.keyword",key)
            .add_should_match("phone.keyword",key)
            .add_should_match("nickname",key)
            .add_must_not_term("user_id.keyword",uid_list)
            .search();

        if(users_json.isArray() == false){
            DEBUG("用户搜索为空")
        }
        // std::cout<<users_json;
        int size = users_json.size();
        std::vector<User> ret_data;
        for(int i=0;i<size;++i){
            User user;
            user.user_id(users_json[i]["_source"]["user_id"].asString());
            user.nickname(users_json[i]["_source"]["nickname"].asString());
            user.phone(users_json[i]["_source"]["phone"].asString());
            user.signature(users_json[i]["_source"]["signature"].asString());
            user.avatar_id(users_json[i]["_source"]["avatar_id"].asString());

            ret_data.push_back(std::move(user));
        }

        return ret_data;
    
    }


private:
    std::shared_ptr<elasticlient::Client> __es_client;
};

class ESMessage{
public:
    using ptr = std::shared_ptr<ESMessage>;
    ESMessage(const std::shared_ptr<elasticlient::Client>& es_client):__es_client(es_client){}

    //创建索引message
    bool createIndex(){
        bool ret = ESIndex(__es_client,"message")
            .append("chat_session_id","standard")
            .append("message_id","standard")
            .append("user_id","standard")
            .append("content")
            .create();
        if(!ret){
            ERROR("ES客户端 创建消息索引失败");
            return false;
        }
        return true;
    }

    bool append(
        const std::string& chat_session_id,
        const std::string& message_id,
        const std::string& user_id,
        const std::string& content
    ){
        bool ret = ESInsert(__es_client,"message")
        .append("chat_session_id",chat_session_id)
        .append("message_id",message_id)
        .append("user_id",user_id)
        .append("content",content)
        .insert(message_id);

        if(!ret){
            ERROR("es 插入{}消息数据失败:",message_id);
            return false;
        }

        return true;
    }

    bool append(const std::shared_ptr<chat_im::Message_ODB>& message){
        return this->append(message->messageSession_id(),message->messageSession_id(),message->sender_id(),message->message_text());
    }

    bool remove(const std::string& message_id){
        bool ret = ESRemove(__es_client,"message")
        .remove(message_id);
        if(!ret){
            ERROR("删除消息{}失败",message_id);
            return false;
        }
        return true;
    }

    std::vector<chat_im::Message_ODB> search(
        const std::string& session_id,
        const std::string& key
    ){
        std::vector<chat_im::Message_ODB> ret;
        Json::Value json_data = ESSearch(__es_client,"message")
            .add_must_term("chat_session_id.keyword",session_id)
            .add_must_match("content",key)
            .search();

        if(json_data.isArray() == false){
            ERROR("搜索{}会话,关键词{}失败",session_id,key);
            return ret;
        }
        int sz = json_data.size();
        DEBUG("检索结果条目数量：{}", sz);
        for (int i = 0; i < sz; i++)
        {
            chat_im::Message_ODB m(
                json_data[i]["_source"]["user_id"].asString(),
                json_data[i]["_source"]["chat_session_id"].asString(),
                json_data[i]["_source"]["message_id"].asString(),
                json_data[i]["_source"]["content"].asString());
            ret.push_back(m);
        }
        return ret;
    }


private:
    std::shared_ptr<elasticlient::Client> __es_client;


};




    
} // namespace chat_im





#endif