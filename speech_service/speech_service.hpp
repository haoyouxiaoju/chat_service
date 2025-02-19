#ifndef SPEECH_SERVICE_HPP
#define SPEECH_SERVICE_HPP

#include <brpc/server.h>
#include <butil/logging.h>

#include "asr.hpp"
#include "etcd.hpp"
#include "logger.hpp"
#include "speech.pb.h"

class SpeechServiceImpl: public chat_im::SpeechService{
public:
    SpeechServiceImpl(const util::asrClient::ptr& client):__asr_client(client){}
    ~SpeechServiceImpl(){}
    void SpeechRecognition(google::protobuf::RpcController *controller,
                           const ::chat_im::SpeechRecognitionReq *request,
                           ::chat_im::SpeechRecognitionRsp *response,
                           ::google::protobuf::Closure *done){
        brpc::ClosureGuard guard(done);

        std::string err;
        std::string res = __asr_client->recognize(request->speech_content(),err);                    
        if(res.empty()){
            ERROR("{} 语言识别发生错误:{}",request->request_id(),err);
            response->set_errmsg(err);
            response->set_success(false);
            response->set_request_id(request->request_id());
            return ;
        }
        response->set_recognition_result(res);
        response->set_success(true);
        response->set_request_id(request->request_id());
    }     

private:
    util::asrClient::ptr __asr_client;
};

class SpeechService{
public:
    using ptr = std::shared_ptr<SpeechService>;
    SpeechService(const util::asrClient::ptr& asr_client,  \
            const util::Registry::ptr& reg_client,          \
            std::shared_ptr<brpc::Server> rpc_service)
            :__asr_client(asr_client),  \
             __reg_client(reg_client),  \
             __rpc_service(rpc_service){}
    ~SpeechService(){}
    void start(){
        __rpc_service->RunUntilAskedToQuit();
    }

private:
    util::asrClient::ptr __asr_client;
    util::Registry::ptr __reg_client;
    std::shared_ptr<brpc::Server> __rpc_service;

};

class SpeechServiceBuilder{
public:
    void make_asrClient(const std::string& app_id,    \
                const std::string& api_key, \
                const std::string& secret_key){
        __asr_client = std::make_shared<util::asrClient>(app_id,api_key,secret_key);
    }
    void make_regClient(const std::string &reg_host,    \
                        const std::string& service_name,     \
                        const std::string& access_host){
        __reg_client =std::make_shared<util::Registry>(reg_host);
        __reg_client->registry(service_name,access_host);
    }
    void make_rpcService(uint16_t port,int32_t timeout,uint8_t num_threads){

        //
        //
        if(!__asr_client){
            ERROR("未初始化语言识别模块");
            abort();
        }

        __rpc_service = std::make_shared<brpc::Server>();

        SpeechServiceImpl* impl = new SpeechServiceImpl(__asr_client);
        bool ret =__rpc_service->AddService(impl,brpc::ServiceOwnership::SERVER_OWNS_SERVICE);
        if(ret == -1){
            ERROR("rpc服务添加失败");
            abort();
        }


        brpc::ServerOptions opt;
        opt.idle_timeout_sec = timeout;
        opt.num_threads = num_threads;
        ret = __rpc_service->Start(port,&opt);
        if(ret == -1){
            ERROR("服务启动失败");
            abort();
        }
    }

    SpeechService::ptr build(){
        if(!__asr_client){
            ERROR("未初始化语言识别模块");
            abort();
        }
        if(!__reg_client){
            ERROR("未初始化服务注册模块");
            abort();
        }
        if(!__rpc_service){
            ERROR("未初始化rpc服务器模块");
            abort();
        }
        return std::make_shared<SpeechService>(__asr_client,__reg_client,__rpc_service);
    }


private:
    util::asrClient::ptr __asr_client;
    util::Registry::ptr __reg_client;
    std::shared_ptr<brpc::Server> __rpc_service;
};


#endif