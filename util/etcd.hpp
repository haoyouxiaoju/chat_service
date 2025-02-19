#ifndef ETCD_HPP
#define ETCD_HPP

#include <etcd/Client.hpp>
#include <etcd/Watcher.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Response.hpp>

#include "logger.hpp"

namespace util{

//注册服务
class Registry{

public:

    using ptr = std::shared_ptr<Registry>;
    Registry(const std::string &host):
        __client(std::make_shared<etcd::Client>(host)),
        __keep_alive(__client->leasekeepalive(3).get()),
        __lease_id(__keep_alive->Lease())
    {}

    //
    //注册
    bool registry(const std::string& key, const std::string& value){
        etcd::Response resp = __client->put(key,value,__lease_id).get();
        if(resp.is_ok() == false ){
            ERROR("注册数据失败:{}",resp.error_message());
            return false;
        }
        return true;
    }
    ~Registry(){__keep_alive->Cancel();}

private:

private:
    std::shared_ptr<etcd::Client> __client;
    std::shared_ptr<etcd::KeepAlive> __keep_alive;
    uint64_t __lease_id;



};


//发现服务
class Discovery{

public:
    using ptr = std::shared_ptr<Discovery>;
    using NotifyCallBack = std::function<void(std::string,std::string)>;
    Discovery(const std::string& host,const std::string& base_dir,  \
        const NotifyCallBack& put_cb,const NotifyCallBack& del_cb):
            __client(std::make_shared<etcd::Client>(host)),
            __put_cb(put_cb),
            __del_cb(del_cb)
            {
                //获取当前已有数据
                etcd::Response resp = __client->ls(base_dir).get();
                if(resp.is_ok() == false){
                    ERROR("获取服务信息失败:{}}",resp.error_message());
                    return ;
                }
                int sz = resp.keys().size();
                for(int i=0;i<sz;++i){
                    if(__put_cb)
                        __put_cb(resp.key(i),resp.value(i).as_string());
                }

                //事件监控
                __watcher = std::make_shared<etcd::Watcher>(*__client.get(),base_dir,   \
                std::bind(&Discovery::callback,this,std::placeholders::_1), true);


            }
    ~Discovery(){}
private:

    void callback(const etcd::Response& resp){
        //
        //回调函数,对事件的管理
        if(resp.is_ok() == false){
            ERROR("收到一个错误的事件通知:{}",resp.error_message());
            return;
        }
        for(auto const& e : resp.events()){
            switch(e.event_type()){
                case etcd::Event::EventType::PUT :{
                    if(__put_cb){
                        __put_cb(e.kv().key(),e.kv().as_string());
                        DEBUG("新增服务:{}-{}",e.kv().key(),e.kv().as_string());
                    }

                    break;
                }
                case etcd::Event::EventType::DELETE_ :{
                    if(__del_cb){
                        __del_cb(e.prev_kv().key(),e.prev_kv().as_string());
                        DEBUG("下线服务:{}-{}",e.prev_kv().key(),e.prev_kv().as_string());
                    }

                    break;
                }
            }
        }

    }

private:

    NotifyCallBack __put_cb;
    NotifyCallBack __del_cb;
    std::shared_ptr<etcd::Client> __client;
    std::shared_ptr<etcd::Watcher> __watcher;

};

}

#endif