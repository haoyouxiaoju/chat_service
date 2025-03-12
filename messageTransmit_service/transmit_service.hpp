#include <brpc/server.h>

#include "channel.hpp"
#include "etcd.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include "rabbitmq.hpp"

#include "mysql/sql_chat_session_member.hpp"
#include "mysql/sql_chat_session.hpp"

#include "base.pb.h"
#include "transmit.pb.h"
#include "friend.pb.h"
#include "user.pb.h"


namespace PROTOBUF_NAMESPACE_ID = google::protobuf;

class transmitServiceImpl : public chat_im::MsgTransmitService{
public:

    transmitServiceImpl(
        const chat_im::util::ServiceChannelManager::ptr& channel_manager,
        const std::shared_ptr<odb::core::database>& db,
        const chat_im::util::MQClient::ptr mq_client,
        const std::string& user_service_name,
        const std::string& exchange_name,
        const std::string& rounting_key
    ):__channel_manager(channel_manager),
    __chat_session_member_manager(std::make_shared<chat_im::util::ChatSessionMemberTable>(db)),
    __mq_manager(mq_client),__user_service_name(user_service_name),
    __exchange_name(exchange_name),__rounting_key(rounting_key){}
    ~transmitServiceImpl(){}

    void GetTransmitTarget(::PROTOBUF_NAMESPACE_ID::RpcController* controller,
        const ::chat_im::NewMessageReq* request,
        ::chat_im::GetTransmitTargetRsp* response,
        ::google::protobuf::Closure* done){
            brpc::ClosureGuard guard(done);

            const std::string request_id = request->request_id();
            const std::string user_id = request->user_id();
            const std::string chat_session_id = request->chat_session_id();

            response->set_request_id(request_id);

            std::function<void(const std::string&)> err_response    \
                = [response](const std::string& err_msg)->void{
                    response->set_errmsg(err_msg);
                    response->set_success(false);
                };

            //获取用户服务模块 通道
            chat_im::util::ServiceChannel::ChannelPtr channel   \
                =  __channel_manager->choose(__user_service_name); 
            if(!channel){
                ERROR("未找到用户服务模块");
                return err_response("未找到用户服务模块");
            }

            //获取发送者的用户信息
            chat_im::GetUserInfoReq req;
            req.set_user_id(user_id);
            req.set_request_id(request_id);

            brpc::Controller cntl;
            chat_im::UserService_Stub stub(channel.get());
            chat_im::GetUserInfoRsp rsp;
            stub.GetUserInfo(&cntl,&req,&rsp,nullptr);

            if(cntl.Failed() == true || !rsp.success()){
                ERROR("获取{}用户信息失败",user_id);
                return err_response("获取用户信息失败块");
            }

            //获取聊天会话内成员id
            std::vector<chat_im::chatSessionMember> member_list     \
                =  __chat_session_member_manager->select_chat_session_id(chat_session_id);   
            std::unordered_set<std::string> member_userId_list;
            for(auto i = member_list.begin();i!=member_list.end();++i){
                member_userId_list.insert(i->user_id());
            }
            //去除发送者的id
            member_userId_list.erase(user_id);
            if(member_list.empty()){
                ERROR("获取{}会话用户失败,成员个数0",chat_session_id);
                return err_response("获取{}会话用户失败,成员个数0");
            }

            chat_im::MessageInfo info;
            const std::string uuid = chat_im::util::uuid();
            info.set_message_id(uuid);
            DEBUG("会话id{}",uuid);
            info.set_chat_session_id(chat_session_id);
            info.set_timestamp(time(nullptr));
            info.mutable_sender()->CopyFrom(rsp.user_info());
            info.mutable_data()->CopyFrom(request->message());
            DEBUG("消息内容:{}",info.data().string_message().content());

            bool isOk = __mq_manager->publish(__exchange_name,info.SerializeAsString(),__rounting_key);
            if(!isOk){
                ERROR("{}发送{}的消息存储失败",user_id,chat_session_id);
                return err_response("消息存储失败");
            }

            response->mutable_message()->CopyFrom(info);
            for(const std::string& str : member_userId_list){
                response->add_target_id_list(str);
            }
            response->set_success(true);


        }

private:
        chat_im::util::ServiceChannelManager::ptr __channel_manager;
        chat_im::util::ChatSessionMemberTable::ptr __chat_session_member_manager;

        chat_im::util::MQClient::ptr __mq_manager;

        std::string __user_service_name;
        std::string __exchange_name;
        std::string __rounting_key;

        
};

class transmitService{
public:
    using ptr = std::shared_ptr<transmitService>;
    transmitService(
        const chat_im::util::Discovery::ptr& discovery,
        const chat_im::util::Registry::ptr & registry,
        const std::shared_ptr<brpc::Server>& transmit_service
    ):__discovery(discovery),__registry(registry),__transmit_service(transmit_service)
    {}
    ~transmitService(){}
    void start(){
        __transmit_service->RunUntilAskedToQuit();
    }
private:
    chat_im::util::Discovery::ptr __discovery;
    chat_im::util::Registry::ptr __registry;
    std::shared_ptr<brpc::Server> __transmit_service;
};

class transmitServiceBuilder{
public:
    void make_regClient(const std::string &reg_host,
                        const std::string &service_name,
                        const std::string &access_host)
    {
        __registry = std::make_shared<chat_im::util::Registry>(reg_host);
        __registry->registry(service_name, access_host);
    }
    
        // 构建ServiceChannel管理对象,关注文件服务,同时去寻找服务
        void makeChannelManager(
            const std::string &etcd_host,
            const std::string &base_service,
            const std::string &user_service_name)
        {
                __user_service_name = user_service_name;
                __channel_manager = std::make_shared<chat_im::util::ServiceChannelManager>();
                __channel_manager->declared(__user_service_name);

                std::function<void(const std::string &, const std::string &)>
                    put_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOnline, __channel_manager.get(), std::placeholders::_1, std::placeholders::_2);
                std::function<void(const std::string &, const std::string &)>
                    del_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOffline, __channel_manager.get(), std::placeholders::_1, std::placeholders::_2);
                __discovery = std::make_shared<chat_im::util::Discovery>(etcd_host, base_service, put_cb, del_cb);
        }

        void make_database_manager(
            const std::string &user,
            const std::string &password,
            const std::string &host,
            const std::string &db,
            const std::string &socket,
            int port,
            int conn_pool_count)
        {
                // 创建mysql的链接
                __db = chat_im::util::ODBFactory::create(user, password, host, db, socket, port, conn_pool_count);
        }

        void make_mq_client(
            const std::string& user,
            const std::string& password,
            const std::string& host,
            const std::string& exchange_name,
            const std::string& queue_name,
            const std::string& routing_key           
        ){
            __exchange_name = exchange_name;
            __rounting_key = routing_key;
            __mq_client = std::make_shared<chat_im::util::MQClient>(user,password,host);
            __mq_client->declareComponents(exchange_name,queue_name,routing_key);
        }

        void make_transmit_service(uint16_t port, int32_t timeout, uint8_t num_threads)
        {
                if (!__channel_manager)
                {
                        ERROR("serviceChannel管理对象未构建");
                        abort();
                }
                if (!__db)
                {
                        ERROR("mysql数据库管理对象未构建");
                        abort();
                }
                if (!__mq_client)
                {
                    ERROR("rabbitMQ 未构建");
                    abort();
                }

                __transmit_service = std::make_shared<brpc::Server>();

                transmitServiceImpl *impl = new transmitServiceImpl(
                    __channel_manager,__db,__mq_client,__user_service_name,__exchange_name,__rounting_key
                );

                bool isOk = __transmit_service->AddService(impl, brpc::ServiceOwnership::SERVER_OWNS_SERVICE);
                if (isOk == -1)
                {
                        ERROR("rpc服务添加失败");
                        abort();
                }

                brpc::ServerOptions opt;
                opt.idle_timeout_sec = timeout;
                opt.num_threads = num_threads;
                isOk = __transmit_service->Start(port, &opt);
                if (isOk == -1)
                {
                        ERROR("rpc服务启动失败");
                        abort();
                }

        }

        transmitService::ptr build(){
            if (!__discovery)
            {
                ERROR("未初始化服务发现模块！");
                abort();
            }
            if (!__transmit_service)
            {
                ERROR("未初始化rpc服务器模块");
                abort();
            }
            if (!__registry)
            {
                ERROR("未初始化服务注册模块");
                abort();
            }
    
            return std::make_shared<transmitService>(__discovery,__registry,__transmit_service);
    
        }

private:
    chat_im::util::Discovery::ptr __discovery;
    chat_im::util::Registry::ptr __registry;
    std::shared_ptr<brpc::Server> __transmit_service;

    chat_im::util::ServiceChannelManager::ptr __channel_manager;
    std::shared_ptr<odb::core::database> __db;
    chat_im::util::MQClient::ptr __mq_client;
    std::string __user_service_name;
    std::string __exchange_name;
    std::string __rounting_key;
};