#ifndef MESSAGE_SERVICE_HPP
#define MESSAGE_SERVICE_HPP

#include <vector>

#include <brpc/server.h>
#include <butil/logging.h>
#include <boost/date_time/posix_time/ptime.hpp>

#include "mysql/sql_message.hpp"
#include "data_es.hpp" 
#include "message.hxx"
#include "message-odb.hxx"

#include "logger.hpp"
#include "channel.hpp"
#include "etcd.hpp"
#include "rabbitmq.hpp"
#include "util/utils.hpp"

#include "message.pb.h"
#include "file.pb.h"
#include "user.pb.h"
#include "base.pb.h"

namespace PROTOBUF_NAMESPACE_ID = ::google::protobuf;

class MessageServiceImpl: public chat_im::MsgStorageService{
    
public:
    MessageServiceImpl(
        const chat_im::util::MessageTable::ptr& message_manager,
        const chat_im::util::ESMessage::ptr& es_message_manager,
        const std::string& user_service_name,
        const std::string& file_service_name,
        const chat_im::util::ServiceChannelManager::ptr& channel_manager
    ):__message_manager(message_manager),__es_message_manager(es_message_manager),
      __user_service_name(user_service_name),__file_service_name(file_service_name),
      __channel_manager(channel_manager)
    {}

    ~MessageServiceImpl(){}

    void GetHistoryMsg(
        ::PROTOBUF_NAMESPACE_ID::RpcController *controller,
        const ::chat_im::GetHistoryMsgReq *request,
        ::chat_im::GetHistoryMsgRsp *response,
        ::google::protobuf::Closure *done)
    {
        brpc::ClosureGuard guard(done);

        std::function<void(const std::string&)> err_response    \
            =[response](const std::string& err_msg)->void{
                response->set_success(false);
                response->set_errmsg(err_msg);
                return;
            };

        const std::string request_id = request->request_id();
        const std::string chat_session_id = request->chat_session_id();
        const std::string user_id = request->user_id();
        const boost::posix_time::ptime start_time =boost::posix_time::from_time_t(request->start_time());
        const boost::posix_time::ptime over_time =boost::posix_time::from_time_t(request->over_time());
        
        response->set_request_id(request_id);

        //获取消息列表
        std::vector<chat_im::Message_ODB>  message_data \
            = __message_manager->select_by_time(chat_session_id,start_time,over_time);
        //如果查询到的消息列表为空,则直接返回
        if(message_data.empty()){
            response->set_success(true);
            return;
        }
        //获取消息中的文件id和发送者id,组成列表
        std::unordered_set<std::string> file_id_set,sender_id_set;
        for(int i=0;i<message_data.size();++i){
            sender_id_set.insert(message_data[i].sender_id());
            if(!message_data[i].file_id().empty()){
                file_id_set.insert(message_data[i].file_id());
                DEBUG("需要下载的文件ID: {}", message_data[i].file_id());
            }
        }

        //从文件服务中获取到文件内容
        std::unordered_map<std::string,std::string> file_data_list;
        bool isOk = __downloadFiles(request_id,file_id_set,file_data_list);
        if(!isOk){
            ERROR("{}从文件服务模块中批量获取文件失败",request_id);
            return err_response("批量获取文件失败");
        }
        
        //从用户服务模块中获取用户信息
        std::unordered_map<std::string,chat_im::UserInfo> user_info_list;
        isOk = __getMultiUserInfo(request_id,sender_id_set,user_info_list);
        if(!isOk){
            ERROR("{}从用户服务模块中获取多名用户信息失败",request_id);
            return err_response("获取用户信息失败");
        }            
        
        //根据前面获取到的信息构建消息信息,最后完成报文
        for(int i =0 ; i<message_data.size();++i){
            chat_im::MessageInfo* info = response->add_message_list();
            info->set_message_id(message_data[i].message_id());
            info->set_chat_session_id(message_data[i].messageSession_id());
            info->set_timestamp(boost::posix_time::to_time_t(message_data[i].create_time()));
            info->mutable_sender()->CopyFrom(user_info_list[message_data[i].sender_id()]);
            //message_type 设定与 pb文件中的类型设置相同
            chat_im::MessageContent* data= info->mutable_data();
            switch(message_data[i].message_type()){
                case chat_im::MessageType::STRING:
                {
                    data->set_message_type(chat_im::MessageType::STRING);
                    data->mutable_string_message()->set_content(message_data[i].message_text());
                    break;
                }
                case chat_im::MessageType::IMAGE:
                {
                    data->set_message_type(chat_im::MessageType::IMAGE);
                    data->mutable_image_message()->set_file_id(message_data[i].file_id());
                    data->mutable_image_message()->set_image_content(file_data_list[message_data[i].file_id()]);
                    break;
                }
                case chat_im::MessageType::SPEECH:
                {
                    data->set_message_type(chat_im::MessageType::SPEECH);
                    data->mutable_speech_message()->set_file_id(message_data[i].file_id());
                    data->mutable_speech_message()->set_file_contents(file_data_list[message_data[i].file_id()]);
                    break;
                }
                case chat_im::MessageType::FILE:
                {
                    data->set_message_type(chat_im::MessageType::FILE);
                    data->mutable_file_message()->set_file_id(message_data[i].file_id());
                    data->mutable_file_message()->set_file_size(message_data[i].file_size());
                    data->mutable_file_message()->set_file_name(message_data[i].file_name());
                    data->mutable_file_message()->set_file_contents(file_data_list[message_data[i].file_id()]);
                    break;
                }
                default:
                {
                    ERROR("获取到的消息类型-{}-错误",message_data[i].message_type());
                    return err_response("消息类型错误");
                }
            }
        }
        
        response->set_success(true);            
    }

    void GetRecentMsg(
        ::PROTOBUF_NAMESPACE_ID::RpcController *controller,
        const ::chat_im::GetRecentMsgReq *request,
        ::chat_im::GetRecentMsgRsp *response,
        ::google::protobuf::Closure *done){
        
        brpc::ClosureGuard guard(done);

        std::function<void(const std::string&)> err_response    \
            =[response](const std::string& err_msg)->void{
                response->set_success(false);
                response->set_errmsg(err_msg);
                return;
            };

        const std::string request_id = request->request_id();
        const std::string chat_session_id = request->chat_session_id();
        const int msg_count = request->msg_count();

        //设置请求id
        response->set_request_id(request_id);

        //获取消息
        std::vector<chat_im::Message_ODB> message_data = __message_manager->select_numberMessage(chat_session_id,msg_count);
        if(message_data.empty()){
            //
            response->set_success(true);
            return ;            
        } 
        //获取消息中的文件id和发送者id,组成列表
        std::unordered_set<std::string> file_id_set,sender_id_set;
        for(int i=0;i<message_data.size();++i){
            sender_id_set.insert(message_data[i].sender_id());
            if(!message_data[i].file_id().empty()){
                file_id_set.insert(message_data[i].file_id());
                DEBUG("需要下载的文件ID: {}", message_data[i].file_id());
            }
        }

        //从文件服务中获取到文件内容
        std::unordered_map<std::string,std::string> file_data_list;
        bool isOk = __downloadFiles(request_id,file_id_set,file_data_list);
        if(!isOk){
            ERROR("{}从文件服务模块中批量获取文件失败",request_id);
            return err_response("批量获取文件失败");
        }
        
        //从用户服务模块中获取用户信息
        std::unordered_map<std::string,chat_im::UserInfo> user_info_list;
        isOk = __getMultiUserInfo(request_id,sender_id_set,user_info_list);
        if(!isOk){
            ERROR("{}从用户服务模块中获取多名用户信息失败",request_id);
            return err_response("获取用户信息失败");
        }            
        
        //根据前面获取到的信息构建消息信息,最后完成报文
        for(int i =0 ; i<message_data.size();++i){
            chat_im::MessageInfo* info = response->add_message_list();
            info->set_message_id(message_data[i].message_id());
            info->set_chat_session_id(message_data[i].messageSession_id());
            info->set_timestamp(boost::posix_time::to_time_t(message_data[i].create_time()));
            info->mutable_sender()->CopyFrom(user_info_list[message_data[i].sender_id()]);
            //message_type 设定与 pb文件中的类型设置相同
            chat_im::MessageContent* data= info->mutable_data();
            switch(message_data[i].message_type()){
                case chat_im::MessageType::STRING:
                {
                    data->set_message_type(chat_im::MessageType::STRING);
                    data->mutable_string_message()->set_content(message_data[i].message_text());
                    break;
                }
                case chat_im::MessageType::IMAGE:
                {
                    data->set_message_type(chat_im::MessageType::IMAGE);
                    data->mutable_image_message()->set_file_id(message_data[i].file_id());
                    data->mutable_image_message()->set_image_content(file_data_list[message_data[i].file_id()]);
                    break;
                }
                case chat_im::MessageType::SPEECH:
                {
                    data->set_message_type(chat_im::MessageType::SPEECH);
                    data->mutable_speech_message()->set_file_id(message_data[i].file_id());
                    data->mutable_speech_message()->set_file_contents(file_data_list[message_data[i].file_id()]);
                    break;
                }
                case chat_im::MessageType::FILE:
                {
                    data->set_message_type(chat_im::MessageType::FILE);
                    data->mutable_file_message()->set_file_id(message_data[i].file_id());
                    data->mutable_file_message()->set_file_size(message_data[i].file_size());
                    data->mutable_file_message()->set_file_name(message_data[i].file_name());
                    data->mutable_file_message()->set_file_contents(file_data_list[message_data[i].file_id()]);
                    break;
                }
                default:
                {
                    ERROR("获取到的消息类型-{}-错误",message_data[i].message_type());
                    return err_response("消息类型错误");
                }
            }
        }
        
        response->set_success(true);                       
    }

    void MsgSearch(
        ::PROTOBUF_NAMESPACE_ID::RpcController *controller,
        const ::chat_im::MsgSearchReq *request,
        ::chat_im::MsgSearchRsp *response,
        ::google::protobuf::Closure *done)
    {

        brpc::ClosureGuard guard(done);

        std::function<void(const std::string&)> err_response    \
            =[response](const std::string& err_msg)->void{
                response->set_success(false);
                response->set_errmsg(err_msg);
                return;
            };

        const std::string request_id = request->request_id();
        const std::string chat_session_id = request->chat_session_id();
        const std::string search_key = request->search_key();

        std::vector<chat_im::Message_ODB> message_data =  __es_message_manager->search(chat_session_id,search_key);
        if(message_data.empty()){
            //
            response->set_success(true);
            return ;            
        } 

        //获取消息中的发送者id,组成列表
        std::unordered_set<std::string> file_id_set,sender_id_set;
        for(int i=0;i<message_data.size();++i){
            sender_id_set.insert(message_data[i].sender_id());
        }
        
        //从用户服务模块中获取用户信息
        std::unordered_map<std::string,chat_im::UserInfo> user_info_list;
        bool isOk = __getMultiUserInfo(request_id,sender_id_set,user_info_list);
        if(!isOk){
            ERROR("{}从用户服务模块中获取多名用户信息失败",request_id);
            return err_response("获取用户信息失败");
        }            
        
        //根据前面获取到的信息构建消息信息,最后完成报文
        //由于是文本搜索,所以只对文本内容进行补充,不对文件内容进行补充
        for(int i =0 ; i<message_data.size();++i){
            chat_im::MessageInfo* info = response->add_message_list();
            info->set_message_id(message_data[i].message_id());
            info->set_chat_session_id(message_data[i].messageSession_id());
            info->set_timestamp(boost::posix_time::to_time_t(message_data[i].create_time()));
            info->mutable_sender()->CopyFrom(user_info_list[message_data[i].sender_id()]);
            //message_type 设定与 pb文件中的类型设置相同
            chat_im::MessageContent* data= info->mutable_data();
            switch(message_data[i].message_type()){
                case chat_im::MessageType::STRING:
                {
                    data->set_message_type(chat_im::MessageType::STRING);
                    data->mutable_string_message()->set_content(message_data[i].message_text());
                    break;
                }
                default:
                {
                    ERROR("获取到的消息类型-{}-错误",message_data[i].message_type());
                    return err_response("消息类型错误");
                }
            }
        }
        response->set_success(true);                       

    }

    void onMessage(const char* message,size_t size){
        if(size<0){
            return;
        }
        chat_im::MessageInfo info;
        bool isOk = info.ParseFromArray(message,size);
        if(!isOk){
            ERROR("消息反序列化失败");
            return;
        }    
        
        //
        std::string file_id,file_name;
        int64_t file_size;            
        switch(info.data().message_type()){
            case chat_im::MessageType::STRING:
            {
                // DEBUG("消息内容{}",info.data().string_message().content());
                isOk = __es_message_manager->append(
                    info.chat_session_id(),
                    info.message_id(),
                    info.sender().user_id(),
                    info.data().string_message().content()  
                );
                if(!isOk){
                    ERROR("{}消息插入失败",info.message_id());
                    return ;
                }
                break;
            }         
            case chat_im::MessageType::IMAGE:
            {                
                isOk = __uploadFile("",info.data().image_message().image_content(),"",info.data().image_message().image_content().size(),file_id);
                file_id = chat_im::util::uuid();
                if(!isOk){
                    ERROR("{}图片上传文件服务系统失败",info.message_id());
                    return ;
                }
                break;
            }  
            case chat_im::MessageType::FILE:
            {
                file_name = info.data().file_message().file_name();
                file_size = info.data().file_message().file_size();
                file_id = chat_im::util::uuid();
                isOk = __uploadFile("",info.data().file_message().file_contents(),file_name,info.data().file_message().file_contents().size(),file_id);
                if(!isOk){
                    ERROR("{}文件上传文件服务系统失败",info.message_id());
                    return ;
                }
                break;      
            } 
            case chat_im::MessageType::SPEECH:
            {
                isOk = __uploadFile("",info.data().speech_message().file_contents(),"",info.data().speech_message().file_contents().size(),file_id);
                file_id = chat_im::util::uuid();
                if(!isOk){
                    ERROR("{}语音文件上传文件服务系统失败",info.message_id());
                    return ;
                }
                break;
            }          
        }
        
        chat_im::Message_ODB message_odb(
                info.sender().user_id(),
                info.chat_session_id(),  
                boost::posix_time::from_time_t(info.timestamp()),
                info.data().message_type()
        ) ;
        message_odb.message_id(info.message_id());
        message_odb.message_text(info.data().string_message().content());
        message_odb.file_id(file_id);
        message_odb.file_name(file_name);
        message_odb.file_size(file_size);
        isOk = __message_manager->insert(message_odb);
        if(!isOk){
            ERROR("向数据库插入新消息失败!");
            return;
        }

    }

private:

    bool __downloadFiles(
        const std::string& request_id,
        const std::unordered_set<std::string>& file_id_list,
        std::unordered_map<std::string,std::string>& file_content_list
    ){
        chat_im::util::ServiceChannel::ChannelPtr channel = __channel_manager->choose(__file_service_name);
        if(!channel){
            ERROR("未找到文件服务模块");
            return false;
        }
        chat_im::GetMultiFileReq req;
        req.set_request_id(request_id);
        for(const std::string& file_id : file_id_list){
            req.add_file_id_list(file_id);
        }
        chat_im::FileService_Stub stub(channel.get());
        brpc::Controller  cntl;
        chat_im::GetMultiFileRsp  rsp;
        stub.GetMultiFile(&cntl,&req,&rsp,nullptr);

        //请求失败
        if(cntl.Failed() ||!rsp.success()){
            ERROR("文件获取失败,失败原因:{}",rsp.errmsg());
            return false;
        }
        
        //处理获取到的文件内容并返回
        for(const auto& [id,content] :rsp.file_data()){
            file_content_list.insert(std::pair(id,content.file_content()));        
        }

        return true;
    
    }

    bool __getMultiUserInfo(
        const std::string& request_id,
        const std::unordered_set<std::string>& user_id_list,
        std::unordered_map<std::string,chat_im::UserInfo>& user_info_list
    )
    {
        chat_im::util::ServiceChannel::ChannelPtr channel = __channel_manager->choose(__user_service_name);
        if(!channel){
            ERROR("未找到用户管理服务模块");
            return false;
        }

        chat_im::GetMultiUserInfoReq req;
        req.set_request_id(request_id);
        for(const std::string& user_id: user_id_list){
           req.add_users_id(user_id);         
        }
        
        chat_im::UserService_Stub stub(channel.get());
        brpc::Controller cntl;
        chat_im::GetMultiUserInfoRsp rsp;
        stub.GetMultiUserInfo(&cntl,&req,&rsp,nullptr);

        //请求失败
        if(cntl.Failed() ||!rsp.success()){
            ERROR("用户消息获取失败,失败原因:{}",rsp.errmsg());
            return false;
        }
        
        for(const auto& [id,info] : rsp.users_info()){
            user_info_list.insert(std::pair(id,info));
        }
        
        return true;
    }

    bool __uploadFile(
        const std::string& request_id,
        const std::string& file_content,
        const std::string& file_name,
        int64_t file_size,
        std::string& file_id
    ){
        chat_im::util::ServiceChannel::ChannelPtr channel = __channel_manager->choose(__file_service_name);
        if(!channel){
            ERROR("未寻找到文件服务模块");
            return false;
        }
         chat_im::FileService_Stub stub(channel.get());
         chat_im::PutSingleFileReq req;
         req.set_request_id(request_id);
         chat_im::FileUpLoadData* data = req.mutable_file_data();  
         data->set_file_content(file_content);
         data->set_file_name(file_name);
         data->set_file_size(file_size);
         chat_im::PutSingleFileRsp rsp;
         brpc::Controller cntl;
         stub.PutSingleFile(&cntl,&req,&rsp,nullptr);
         if(cntl.Failed() || !rsp.success()){
            ERROR("{}文件上传失败{}",file_name,rsp.errmsg());
            return false;
         }
        file_id = rsp.file_info().file_id();

        return true;
        

    
    }

private:
    chat_im::util::MessageTable::ptr __message_manager;
    chat_im::util::ESMessage::ptr __es_message_manager;
    std::string __user_service_name;
    std::string __file_service_name;
    chat_im::util::ServiceChannelManager::ptr __channel_manager;

    
};

class MessageService{
public:
   using ptr = std::shared_ptr<MessageService>;
   MessageService(
       const std::shared_ptr<brpc::Server> &message_service,
       const chat_im::util::Discovery::ptr &discovery,
       const chat_im::util::Registry::ptr &registry,
       const chat_im::util::MQClient::ptr &mq_client
   ): __message_service(message_service),__mq_client(mq_client),
      __discovery(discovery),__registry(registry)
   {
   }
   ~MessageService(){}

   void start(){
       __message_service->RunUntilAskedToQuit(); 
   }
   

private:
   std::shared_ptr<brpc::Server> __message_service;
   chat_im::util::Discovery::ptr __discovery;
   chat_im::util::Registry::ptr __registry;
   chat_im::util::MQClient::ptr __mq_client;

};

class MessageServiceBuilder{
public:
    MessageServiceBuilder(){}
    ~MessageServiceBuilder(){}

    void make_database_manager(
        const std::string& user,
        const std::string& password,
        const std::string& host,
        const std::string& db,
        const std::string& socket,
        int port,
        int conn_pool_count
    ){
      //创建mysql的链接
      std::shared_ptr<odb::core::database>  \
        database_connection (chat_im::util::ODBFactory::create(user,password,host,db,socket,port,conn_pool_count));
      
      //构建mysql用户管理对象
      __message_manager = std::make_shared<chat_im::util::MessageTable>(chat_im::util::MessageTable(database_connection));
    }

    //创建es服务器连接,构建es服务用户管理对象
    void makeESUserManager(const std::vector<std::string>& es_host){
      std::shared_ptr<elasticlient::Client> es_connection (chat_im::util::ESClientFactory::create(es_host));
      __es_message_manager = std::make_shared<chat_im::util::ESMessage>(es_connection);
    }

    //构建ServiceChannel管理对象,关注文件服务,同时去寻找服务
    void makeChannelManager(
      const std::string& etcd_host,
      const std::string& base_service,
      const std::string& file_service_name,
      const std::string& user_service_name
    ){
      __file_service_name =file_service_name;
      __user_service_name = user_service_name;
      __channel_manager = std::make_shared<chat_im::util::ServiceChannelManager>();
      __channel_manager->declared(__file_service_name);
      __channel_manager->declared(__user_service_name);

      std::function<void(const std::string&,const std::string&)>    \
        put_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOnline,__channel_manager.get(),std::placeholders::_1,std::placeholders::_2);
      std::function<void(const std::string&,const std::string&)>    \
        del_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOffline,__channel_manager.get(),std::placeholders::_1,std::placeholders::_2);
      
      __discovery = std::make_shared<chat_im::util::Discovery>(etcd_host,base_service,put_cb,del_cb);

    }     
      
    void make_regClient(const std::string &reg_host,    \
                        const std::string& service_name,     \
                        const std::string& access_host){
        __registry =std::make_shared<chat_im::util::Registry>(reg_host);
        __registry->registry(service_name,access_host);
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
        __queue_name = queue_name;
        __mq_client = std::make_shared<chat_im::util::MQClient>(user,password,host);
        __mq_client->declareComponents(exchange_name,queue_name,routing_key);
    }

    void make_message_service(uint16_t port,int32_t timeout,uint8_t num_threads){
        if (!__channel_manager)
        {
            ERROR("serviceChannel管理对象未构建");
            abort();
        }
        if (!__message_manager)
        {
            ERROR("mysql数据库管理对象未构建");
            abort();
        }
        if (!__es_message_manager)
        {
            ERROR("es服务管理对象未构建");
            abort();
        }
        if (!__mq_client)
        {
            ERROR("rabbitMQ 未构建");
            abort();
        }

        __message_service = std::make_shared<brpc::Server>();

        MessageServiceImpl* impl = new MessageServiceImpl(
           __message_manager,__es_message_manager  ,
           __user_service_name,__file_service_name,
           __channel_manager       
        );

        bool isOk = __message_service->AddService(impl, brpc::ServiceOwnership::SERVER_OWNS_SERVICE);
        if (isOk == -1)
        {
            ERROR("rpc服务添加失败");
            abort();
        }

        brpc::ServerOptions opt;
        opt.idle_timeout_sec = timeout;
        opt.num_threads = num_threads;
        isOk = __message_service->Start(port, &opt);
        if (isOk == -1)
        {
            ERROR("rpc服务启动失败");
            abort();
        }

        chat_im::util::MQClient::MessageCallback cb =  std::bind(&MessageServiceImpl::onMessage,impl,std::placeholders::_1,std::placeholders::_2);
        __mq_client->consume(__queue_name,cb);
    }

    MessageService::ptr build(){
        if (!__discovery)
        {
            ERROR("未初始化服务发现模块！");
            abort();
        }
        if (!__message_service)
        {
            ERROR("未初始化rpc服务器模块");
            abort();
        }
        if (!__registry)
        {
            ERROR("未初始化服务注册模块");
            abort();
        }

        return std::make_shared<MessageService>(__message_service,__discovery,__registry,__mq_client);

    }

private:
   std::shared_ptr<brpc::Server> __message_service;
   std::string __file_service_name;
   std::string __user_service_name;
   std::string __exchange_name;
   std::string __queue_name;
   chat_im::util::ServiceChannelManager::ptr __channel_manager;
   chat_im::util::MessageTable::ptr __message_manager;
   chat_im::util::ESMessage::ptr __es_message_manager;
   chat_im::util::Discovery::ptr __discovery;
   chat_im::util::Registry::ptr __registry;
   chat_im::util::MQClient::ptr __mq_client;
};

#endif