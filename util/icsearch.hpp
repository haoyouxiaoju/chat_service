#ifndef ICSEARCH_HPP
#define ICSEARCH_HPP

#include <elasticlient/client.h>
#include <cpr/cpr.h>
#include <json/json.h>
#include <iostream>
#include <memory>

#include "logger.hpp"

namespace chat_im::util{


//
// 序列化
bool Serialize(const Json::Value& value,std::string& body){
    Json::StreamWriterBuilder swb;
    std::shared_ptr<Json::StreamWriter> sw(swb.newStreamWriter());

    std::stringstream ss;
    bool rest = sw->write(value,&ss);
    if(rest != 0){
        //不成功
        DEBUG("序列化失败");
        return false;
    }
    body = ss.str();
    return true;
}

//
//反序列化
bool unSerivlize(const std::string& body ,Json::Value& value){
    Json::CharReaderBuilder crb;
    std::shared_ptr<Json::CharReader> cr(crb.newCharReader());

    std::string err;
    bool rest = cr->parse(body.c_str(),body.c_str()+body.size(),&value,&err);
    if(rest == false){
        DEBUG("反序列化失败:{}",err);
        return false;
    }
    return true;
}

//
//      索引
class ESIndex{
public:
    ESIndex(std::shared_ptr<elasticlient::Client>& client,    \
                  const std::string &name,const std::string& type="_doc"):
                __name(name),__type(type),__client(client)
                {
                    //settings 的设置
                    Json::Value analysis;
                    Json::Value analyzer;
                    Json::Value ik;
                    Json::Value tokenizer;

                    tokenizer["tokenizer"]="ik_max_word";
                    ik["ik"] = tokenizer;
                    analyzer["analyzer"] = ik;
                    analysis["analysis"]= analyzer;
                    __index["settings"] = analysis;

                }
    ESIndex& append(const std::string& key,const std::string& type = "text",    \
                        const std::string& analyzer ="ik_max_word", bool enabled = true){
            //
            //添加properties的选项
            Json::Value options;
            options["type"] = type;
            options["analyzer"] = analyzer;
            if(enabled == false ) options["enabled"]=enabled;

            __properties[key] = options;

            return *this;
    }

    bool create(){
        Json::Value mappings;
        mappings["dynamic"] = true;
        mappings["properties"] = __properties;

        __index["mappings"] = mappings;

        std::string body;
        bool rest = Serialize(__index,body);
        if(rest == false ){
            ERROR("序列化失败");
            return false;
        }

        try{
            cpr::Response resp = __client->index(__name,__type,"",body);
            if(resp.status_code <200 || resp.status_code>=300){
                ERROR("创建ES索引{}失败,响应码:{}",__name,resp.status_code);
                return false;
            }
        }catch(std::exception& e){
            ERROR("创建ES索引{}失败,错误信息:{}", __name, e.what());
        }

        return true;
    }


private:
    std::string __name;
    std::string __type;
    Json::Value __index;
    Json::Value __properties;
    std::shared_ptr<elasticlient::Client> __client;
};


class ESInsert{
public:
    ESInsert(std::shared_ptr<elasticlient::Client> client,const std::string& name,const std::string& type="_doc"):
        __name(name),__type(type),__client(client){}
    ESInsert& append(const std::string& key,const std::string& value){
        __item[key] = value;
        return *this;
    }
    bool insert(const std::string& id){
        std::string str;
        bool rest = Serialize(__item,str);
        if(rest == false){
            ERROR("序列化失败,无法插入数据");
            return false;
        }

        try{
            cpr::Response resp = __client->index(__name,__type,id,str);
            if(resp.status_code >=300 || resp.status_code <200){
                ERROR("插入失败,响应码:{},请求数据:{}",resp.status_code,str);
                return false;
            }
        }catch(std::exception& e){
            ERROR("插入失败,错误信息:{},请求数据:{}", e.what() , str);
            return false;
        }
        return true;
    }
private:
    std::string __name;
    std::string __type;
    Json::Value __item;
    std::shared_ptr<elasticlient::Client> __client;

};

class ESRemove{
public:
    ESRemove(std::shared_ptr<elasticlient::Client> client,const std::string& name,const std::string& type="_doc"):
        __name(name), __type(type), __client(client){}
    
    bool remove(const std::string& id){
        try{
            cpr::Response resp = __client->remove(__name, __type, id);
            if (resp.status_code >= 300 || resp.status_code < 200)
            {
                ERROR("删除失败,响应码:{}", resp.status_code);
                return false;
            }
        }catch(std::exception& e){
            ERROR("删除失败,错误信息:{}",e.what());
            return false;
        }
        return true;
    }

private:
    std::string __name;
    std::string __type;
    std::shared_ptr<elasticlient::Client> __client;
};

class ESSearch{
public:
    ESSearch(std::shared_ptr<elasticlient::Client> client,const std::string& name,const std::string& type="_doc"):
        __name(name),__type(type),__client(client){}
    ESSearch& add_must_not_term(const std::string& key,const std::vector<std::string> values){
        Json::Value item;
        for(const std::string& value : values){
            item[key].append(value);
        }

        Json::Value terms;
        terms["terms"] = item;
        __must_not_terms.append(terms);
        return *this;
    }
    ESSearch& add_should_match(const std::string& key,const std::string& value){
        Json::Value match;

        Json::Value item;
        item[key]=value;
        match["match"]=item;

        __should_match.append(match);

        return *this;
    }
    ESSearch& add_must_term(const std::string &key, const std::string &val) {
        Json::Value field;
        field[key] = val;
        Json::Value term;
        term["term"] = field;
        __must.append(term);
        return *this;
    }
    ESSearch& add_must_match(const std::string &key, const std::string &val){
        Json::Value field;
        field[key] = val;
        Json::Value match;
        match["match"] = field;
        __must.append(match);
        return *this;
    }

    Json::Value search(){
        Json::Value root;
        Json::Value query;
        Json::Value _bool;

        if (__must_not_terms.empty() == false) _bool["must_not"] = (__must_not_terms);
        if (__should_match.empty() == false) _bool["should"] = (__should_match);
        if (__must.empty() == false) _bool["must"]   = __must;
        query["bool"] = _bool;
        root["query"] = query;

        // std::cout<<root;

        std::string str;
        bool rest = Serialize(root,str);
        if(rest == false){
            ERROR("序列化失败,无法搜索数据");
            return Json::Value();
        }
        
        Json::Value ret_value;
        try
        {
            cpr::Response resp = __client->search(__name, __type, str);
            if (resp.status_code >= 300 || resp.status_code < 200)
            {
                ERROR("搜索失败,响应码:{}", resp.status_code);
                return Json::Value();
            }
            rest = unSerivlize(resp.text, ret_value);
            if (rest == false || ret_value.empty())
            {
                ERROR("反序列化失败,无法获取数据");
                return Json::Value();
            }
        }
        catch (std::exception &e)
        {
            ERROR("搜索失败,错误信息{}",e.what() );
            return Json::Value();
        }

        return ret_value["hits"]["hits"];


    }

private:
    std::string __name;
    std::string __type;
    Json::Value __must_not_terms;
    Json::Value __should_match;
    Json::Value __must;
    std::shared_ptr<elasticlient::Client> __client;

};

}

#endif