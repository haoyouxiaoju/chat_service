#ifndef DATA_ES_HPP
#define DATA_ES_HPP

#include "icsearch.hpp"
#include "logger.hpp"
#include "user.hxx"

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
        int size = users_json.size();
        std::vector<User> ret_data;
        for(int i=0;i<size;++i){
            User user;
            user.user_id(users_json[i]["_source"]["user_id"].asString());
            user.nickname(users_json[i]["_source"]["nickname"].asString());
            user.password(users_json[i]["_source"]["password"].asString());
            user.signature(users_json[i]["_source"]["signature"].asString());
            user.avatar_id(users_json[i]["_source"]["avatar_id"].asString());

            ret_data.push_back(std::move(user));
        }

        return std::move(ret_data);
    
    }



private:
    std::shared_ptr<elasticlient::Client> __es_client;
};






    
} // namespace chat_im





#endif