#pragma once


#include <ev.h> 
#include <amqpcpp.h>
#include <amqpcpp/libev.h>

#include <openssl/ssl.h>
#include <openssl/opensslv.h>

#include "logger.hpp"



namespace chat_im::util{

class MQClient{
public:
    using MessageCallback = std::function<void(const char *, size_t)>;
    using ptr = std::shared_ptr<MQClient>;
    MQClient(
        const std::string &user,
        const std::string &password,
        const std::string &host)
    {
        __loop = EV_DEFAULT;
        __handler = std::make_unique<AMQP::LibEvHandler>(__loop);
        std::string url("amqp://" + user + ":" + password + "@" + host + "/");
        AMQP::Address address(url);
        __connection = std::make_unique<AMQP::TcpConnection>(__handler.get(), address);
        __channel = std::make_unique<AMQP::TcpChannel>(__connection.get());

        __loop_thread = std::thread([this]()
                                    { ev_run(__loop, 0); });
    }

    ~MQClient()
    {
        ev_async_init(&__async_watcher,watcher_callback);
        ev_async_start(__loop,&__async_watcher);
        ev_async_send(__loop,&__async_watcher);
        __loop_thread.join();
        __loop = nullptr;
    }

    void declareComponents(
        const std::string &exchange,
        const std::string& queue,
        const std::string& routing_key = "routing_key",
        AMQP::ExchangeType exchange_type = AMQP::ExchangeType::direct
    ){
        __channel->declareExchange(exchange,exchange_type)
            .onError([](const char* message){
                ERROR("交换机声明失败:{}",message);
                exit(0);
            })  
            .onSuccess([exchange](){
                INFO("{}交换机创建成功",exchange);
            }) ;
        __channel->declareQueue(queue)
            .onError([](const char* message){
                ERROR("队列声明失败:{}",message);
                exit(0);
            })
            .onSuccess([exchange](){
                INFO("{}队列创建成功",exchange);
            });
        __channel->bindQueue(exchange,queue,routing_key)
            .onError([exchange,queue](const char* message){
                ERROR("{}-{} 绑定失败",exchange,queue);
                exit(0);
            })
            .onSuccess([exchange,queue,routing_key](){
                INFO("{}-{}-{} 绑定成功!",exchange,queue,routing_key);
            });
    }

    bool publish(
        const std::string& exchange,
        const std::string& msg,
        const std::string& routing_key = "rounting_key"
    ){
        // DEBUG("向交换机{}-{}发布消息{}",exchange,routing_key,msg);
        bool ret = __channel->publish(exchange,routing_key,msg);
        if(!ret){
            ERROR("{}发布消息失败:",exchange);
            return false;
        }
        return true;
    }

    void consume(
        const std::string& queue,
        const MessageCallback& cb
    ){
        DEBUG("开始订阅{}队列消息",queue);
        __channel->consume(queue,"consume-tag")
            .onReceived([this,cb](
                const AMQP::Message& message,
                uint64_t deliveryTag,
                bool redelivered
            ){
                cb(message.body(),message.bodySize());
                __channel->ack(deliveryTag);
            })
            .onError([queue](const char* message){
                ERROR("订阅{}队列消息失败{}",queue,message);
                exit(0);
            });
    }

private:
    static void watcher_callback(struct ev_loop *loop, ev_async *watcher, int32_t revents) {
            ev_break(loop, EVBREAK_ALL);
    }

private:

    struct ev_async __async_watcher;
    struct ev_loop* __loop;

    std::unique_ptr<AMQP::LibEvHandler> __handler;
    std::unique_ptr<AMQP::TcpConnection> __connection;
    std::unique_ptr<AMQP::TcpChannel> __channel;
    std::thread __loop_thread;

};


}


