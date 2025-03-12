#ifndef CONNECTION_WS_HPP
#define CONNECTION_WS_HPP

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include "logger.hpp"

using server_t = websocketpp::server<websocketpp::config::asio>;

class WSConnection{
public:
    using ptr = std::shared_ptr<WSConnection>;
    struct Client{
        Client(const std::string& uid,const std::string& sid):user_id(uid),session_id(sid){};
        std::string user_id;
        std::string session_id;
    };
    void insert(const server_t::connection_ptr& conn,const std::string& user_id,const std::string& session_id)
    {
        std::lock_guard guard(__mutex);
        __uid_connections.insert(std::make_pair(user_id,conn));
        __connection_clients.insert(std::make_pair(conn,Client(user_id,session_id)));
        DEBUG("新增长连接用户信息：{}-{}-{}", (size_t)conn.get(), user_id, session_id);
    }
    void remove(const server_t::connection_ptr& conn){
        std::lock_guard guard(__mutex);
        auto item = __connection_clients.find(conn);
        //没找到
        if(item == __connection_clients.end()){
            ERROR("删除-未找到长连接 {} 对应的客户端信息！", (size_t)conn.get());
            return;
        }
        __uid_connections.erase(item->second.user_id);
        __connection_clients.erase(item);
        DEBUG("删除长连接信息完毕！");
    }
    server_t::connection_ptr connection(const std::string& uid){
        std::lock_guard guard(__mutex);
        auto item = __uid_connections.find(uid);
        if(item == __uid_connections.end()){
            ERROR("未找到 {} 客户端的长连接！", uid);
            return server_t::connection_ptr();
        }
        return item->second;
    }

    bool client(const server_t::connection_ptr& conn,std::string& uid,std::string& sid){
        std::lock_guard guard(__mutex);
        auto item = __connection_clients.find(conn);
        if(item == __connection_clients.end()){
            ERROR("获取-未找到长连接 {} 对应的客户端信息！", (size_t)conn.get());
            return false;
        }
        uid = item->second.user_id;
        sid = item->second.session_id;
        return true;
    }
private:
    std::mutex __mutex;
    std::unordered_map<std::string,server_t::connection_ptr> __uid_connections;
    std::unordered_map<server_t::connection_ptr,Client> __connection_clients;

};






#endif