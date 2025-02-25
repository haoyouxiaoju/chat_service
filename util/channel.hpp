#ifndef CHANNEL_HPP  
#define CHANNEL_HPP

#include <brpc/channel.h>
#include "logger.hpp"

namespace chat_im::util{


class ServiceChannel{
public:
    using ptr = std::shared_ptr<ServiceChannel>;
    using ChannelPtr = std::shared_ptr<brpc::Channel>;

    ServiceChannel(const std::string& service_name):
        __service_name(service_name)
        {}
    //
    //服务上线一个通道
    void append(const std::string& host){
        ChannelPtr channel = std::make_shared<brpc::Channel>();
        brpc::ChannelOptions options;
        options.protocol = "baidu_std";
        options.connect_timeout_ms = -1;
        options.timeout_ms = -1;
        options.max_retry = 3;
        if (channel->Init(host.c_str(), &options) != 0)
        {
            ERROR("初始化channel失败:{}",host);
            return;
        }
        std::unique_lock<std::mutex> lock(__mutex);
        __channels.push_back(channel);
        __hosts.insert(std::make_pair(host,channel));
    }
    //
    //服务下线一个通道
    void remove(const std::string& host){

        std::unique_lock<std::mutex> lock(__mutex);
        std::unordered_map<std::string,ChannelPtr>::iterator ite =  __hosts.find(host);
        if(ite == __hosts.end()){
            DEBUG("没有找到要下线的通道,不处理返回");
            return;
        }
        for(std::vector<ChannelPtr>::iterator i = __channels.begin();i!= __channels.end();++i){
            if(*i == ite->second){
                __channels.erase(i);
                break;
            }            
        }
        __hosts.erase(ite);

    }

    //
    //获取一个信道并返回
    ChannelPtr choose(){
        std::unique_lock<std::mutex> lock(__mutex);
        if(__channels.size() == 0){
            ERROR("当前{}没有信道,无法获取",__service_name);
            return nullptr;
        }
        int32_t cur_index = (++__index%__channels.size());
        return __channels.at(cur_index);

    }


private:

private:
    std::mutex __mutex;
    int32_t __index;            //  当前轮转下标计数
    std::string __service_name; //  服务名称
    std::vector<ChannelPtr> __channels; //服务对应的信道
    std::unordered_map<std::string,ChannelPtr> __hosts; //主机地址 ==> 信道


};


class ServiceChannelManager{
    public:
        using ptr = std::shared_ptr<ServiceChannelManager>;

        ServiceChannel::ChannelPtr choose(const std::string& service_name){
            std::unique_lock<std::mutex> lock(__mutex);
            std::unordered_map<std::string,ServiceChannel::ptr>::iterator ite = __services.find(service_name);
            if(ite == __services.end()){
                ERROR("当前没有找到-{}-对应的服务",service_name);
                return nullptr;
            }
            return ite->second->choose();
        }
        //
        //  添加关心的服务
        void declared(const std::string& service_name){
            std::unique_lock<std::mutex> lock(__mutex);
            __follow_service.insert(service_name);
        }

        //
        //对关心的服务进行上线  
        void onServiceOnline(const std::string& service_instance,const std::string& host){
            std::string service_name = getServiceName(service_instance);
            ServiceChannel::ptr service;
            {
                std::unique_lock<std::mutex> lock(__mutex);
                //
                // 先检查是否是自己所关心的服务
                std::unordered_set<std::string>::iterator ite = __follow_service.find(service_name);
                if (ite == __follow_service.end())
                {
                    DEBUG("上线无关服务{}", service_name);
                    return;
                }

                std::unordered_map<std::string,ServiceChannel::ptr>::iterator ite_services  \
                                                    = __services.find(service_name);
                if(ite_services != __services.end()){
                    //
                    //当__services已存在了对应的service,则直接使用,否则新建并加入
                    service = ite_services->second;
                }else{
                    service = std::make_shared<ServiceChannel>(service_name);
                    __services.insert(std::make_pair(service_name,service));
                }
            }
            service->append(host);
            DEBUG("{}服务上线{},完成添加",service_name,host);

        }
        void onServiceOffline(const std::string& service_instance,const std::string& host){
            std::string service_name = getServiceName(service_instance);
            ServiceChannel::ptr service;
            {
                std::unique_lock<std::mutex> lock(__mutex);
                //
                // 先检查是否是自己所关心的服务
                std::unordered_set<std::string>::iterator ite = __follow_service.find(service_name);
                if (ite == __follow_service.end())
                {
                    DEBUG("下线无关服务{}", service_name);
                    return;
                }

                std::unordered_map<std::string,ServiceChannel::ptr>::iterator ite_services  \
                                                    = __services.find(service_name);
                if(ite_services == __services.end()){
                    WARN("没有找到{}对应的服务",service_name);
                    return;
                }
                service = ite_services->second;
            }
            service->remove(host);
            DEBUG("{}服务下线{},完成删除",service_name,host);
        }
    private:
        std::string getServiceName(const std::string& service_instance){
            size_t index = service_instance.find_last_of("/");
            if(index  == std::string::npos){
                return service_instance;
            }
            return service_instance.substr(0,index);

        }


    private:
        std::mutex __mutex;
        std::unordered_set<std::string> __follow_service; 
        std::unordered_map<std::string,ServiceChannel::ptr> __services;
};

}

#endif