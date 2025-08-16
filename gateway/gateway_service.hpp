#include "connection.hpp"
#include "httplib.h"

#include "etcd.hpp"
#include "channel.hpp"
#include "rabbitmq.hpp"
#include "data_es.hpp"
#include "data_redis.hpp"
#include "logger.hpp"

#include "base.pb.h"
#include "file.pb.h"
#include "friend.pb.h"
#include "gateway.pb.h"
#include "message.pb.h"
#include "notify.pb.h"
#include "speech.pb.h"
#include "transmit.pb.h"
#include "user.pb.h"



const std::string   GET_PHONE_VERIFY_CODE =  "/service/user/get_phone_verify_code"      ;     
const std::string   USERNAME_LOGIN        =  "/service/user/username_login";
const std::string   PHONE_REGISTER        =  "/service/user/phone_register";
const std::string   PHONE_LOGIN           =  "/service/user/phone_login";
const std::string   GET_USERINFO          =  "/service/user/get_user_info";
const std::string   SET_USER_AVATAR       =  "/service/user/set_avatar";
const std::string   SET_USER_NICKNAME     =  "/service/user/set_nickname";
const std::string   SET_USER_DESC         =  "/service/user/set_description";
const std::string   SET_USER_PHONE        =  "/service/user/set_phone";
const std::string   FRIEND_GET_LIST       =  "/service/friend/get_friend_list";
const std::string   FRIEND_APPLY          =  "/service/friend/add_friend_apply";
const std::string   FRIEND_APPLY_PROCESS  =  "/service/friend/add_friend_process";
const std::string   FRIEND_REMOVE         =  "/service/friend/remove_friend";
const std::string   FRIEND_SEARCH         =  "/service/friend/search_friend";
const std::string   FRIEND_GET_PENDING_EV =  "/service/friend/get_pending_friend_events";
const std::string   CSS_GET_LIST          =  "/service/friend/get_chat_session_list";
const std::string   CSS_CREATE            =  "/service/friend/create_chat_session";
const std::string   CSS_GET_MEMBER        =  "/service/friend/get_chat_session_member";
const std::string   MSG_GET_RANGE         =  "/service/message_storage/get_history";
const std::string   MSG_GET_RECENT        =  "/service/message_storage/get_recent";
const std::string   MSG_KEY_SEARCH        =  "/service/message_storage/search_history";
const std::string   NEW_MESSAGE           =  "/service/message_transmit/new_message";
const std::string   FILE_GET_SINGLE       =  "/service/file/get_single_file";
const std::string   FILE_GET_MULTI        =  "/service/file/get_multi_file";
const std::string   FILE_PUT_SINGLE       =  "/service/file/put_single_file";
const std::string   FILE_PUT_MULTI        =  "/service/file/put_multi_file";
const std::string   SPEECH_RECOGNITION    =  "/service/speech/recognition";

namespace util = chat_im::util;

class gatewayService{
public:
    using ptr = std::shared_ptr<gatewayService>;
    gatewayService(
        int websocket_port,
        int http_port,
        const std::shared_ptr<sw::redis::Redis> &redis_client,
        const util::ServiceChannelManager::ptr &channel_manager,
        const util::Discovery::ptr &service_discovery,
        const std::string &user_service_name,
        const std::string &file_service_name,
        const std::string &speech_service_name,
        const std::string &message_service_name,
        const std::string &transmit_service_name,
        const std::string &friend_service_name
    ):
    __redis_session_manager(std::make_shared<util::Session>(redis_client)),
    __redis_status_manager(std::make_shared<util::Status>(redis_client)),
    __channel_manager(channel_manager),
    __service_discovery(service_discovery),
    __user_service_name(user_service_name),
    __file_service_name(file_service_name),
    __speech_service_name(speech_service_name),
    __message_service_name(message_service_name),
    __transmit_service_name(transmit_service_name),
    __friend_service_name(friend_service_name),
    __ws_connection_manager(std::make_shared<WSConnection>())
    {
        __ws_server.set_access_channels(websocketpp::log::alevel::none);
        __ws_server.init_asio();
        __ws_server.set_open_handler(std::bind(&gatewayService::onOpen, this, std::placeholders::_1));
        __ws_server.set_close_handler(std::bind(&gatewayService::onClose, this, std::placeholders::_1));
        auto wscb = std::bind(&gatewayService::onMessage, this,
                              std::placeholders::_1, std::placeholders::_2);
        __ws_server.set_message_handler(wscb);
        __ws_server.set_reuse_addr(true);
        __ws_server.listen(websocket_port);
        __ws_server.start_accept();

        __http_server.Post(GET_PHONE_VERIFY_CODE   ,(httplib::Server::Handler)std::bind(&gatewayService::PhoneVerifyCode            ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(USERNAME_LOGIN          ,(httplib::Server::Handler)std::bind(&gatewayService::UserLogin                  ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(PHONE_REGISTER          ,(httplib::Server::Handler)std::bind(&gatewayService::PhoneRegister              ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(PHONE_LOGIN             ,(httplib::Server::Handler)std::bind(&gatewayService::PhoneLogin                 ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(GET_USERINFO            ,(httplib::Server::Handler)std::bind(&gatewayService::GetUserInfo                ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(SET_USER_AVATAR         ,(httplib::Server::Handler)std::bind(&gatewayService::SetUserAvatar              ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(SET_USER_NICKNAME       ,(httplib::Server::Handler)std::bind(&gatewayService::SetUserNickname            ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(SET_USER_DESC           ,(httplib::Server::Handler)std::bind(&gatewayService::SetUserDescription         ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(SET_USER_PHONE          ,(httplib::Server::Handler)std::bind(&gatewayService::SetUserPhoneNumber         ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(FRIEND_GET_LIST         ,(httplib::Server::Handler)std::bind(&gatewayService::GetFriendList              ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(FRIEND_APPLY            ,(httplib::Server::Handler)std::bind(&gatewayService::FriendAdd                  ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(FRIEND_APPLY_PROCESS    ,(httplib::Server::Handler)std::bind(&gatewayService::FriendAddProcess           ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(FRIEND_REMOVE           ,(httplib::Server::Handler)std::bind(&gatewayService::FriendRemove               ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(FRIEND_SEARCH           ,(httplib::Server::Handler)std::bind(&gatewayService::FriendSearch               ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(FRIEND_GET_PENDING_EV   ,(httplib::Server::Handler)std::bind(&gatewayService::GetPendingFriendEventList  ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(CSS_GET_LIST            ,(httplib::Server::Handler)std::bind(&gatewayService::GetChatSessionList         ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(CSS_CREATE              ,(httplib::Server::Handler)std::bind(&gatewayService::ChatSessionCreate          ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(CSS_GET_MEMBER          ,(httplib::Server::Handler)std::bind(&gatewayService::GetChatSessionMember       ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(MSG_GET_RANGE           ,(httplib::Server::Handler)std::bind(&gatewayService::GetHistoryMsg              ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(MSG_GET_RECENT          ,(httplib::Server::Handler)std::bind(&gatewayService::GetRecentMsg               ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(MSG_KEY_SEARCH          ,(httplib::Server::Handler)std::bind(&gatewayService::MsgSearch                  ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(NEW_MESSAGE             ,(httplib::Server::Handler)std::bind(&gatewayService::NewMessage                 ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(FILE_GET_SINGLE         ,(httplib::Server::Handler)std::bind(&gatewayService::GetSingleFile              ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(FILE_GET_MULTI          ,(httplib::Server::Handler)std::bind(&gatewayService::GetMultiFile               ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(FILE_PUT_SINGLE         ,(httplib::Server::Handler)std::bind(&gatewayService::PutSingleFile              ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(FILE_PUT_MULTI          ,(httplib::Server::Handler)std::bind(&gatewayService::PutMultiFile               ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_server.Post(SPEECH_RECOGNITION      ,(httplib::Server::Handler)std::bind(&gatewayService::SpeechRecognition          ,this,std::placeholders::_1,std::placeholders::_2)); 
        __http_thread = std::thread([this, http_port](){
            __http_server.listen("0.0.0.0", http_port);
        });
        __http_thread.detach();
    }
    void start() {
        __ws_server.run();
    }
    ~gatewayService(){}


private:
    //ws长连接建立
    void onOpen(websocketpp::connection_hdl hdl){
        DEBUG("{}建立websocket长连接",(size_t)__ws_server.get_con_from_hdl(hdl).get());
    }
    //ws断开长连接 清理工作
    void onClose(websocketpp::connection_hdl hdl){
       //长连接断开时清理工作
       auto conn = __ws_server.get_con_from_hdl(hdl);
       std::string user_id,session_id;
       bool ret = __ws_connection_manager->client(conn,user_id,session_id);
       if(ret == false){
        WARN("长连接断开,未找到长连接对应的客户端消息");
        return ;
       } 
       //移除登录会话
       __redis_session_manager->remove(session_id);
       __redis_status_manager->remove(user_id);
       __ws_connection_manager->remove(conn);
       DEBUG("{} {} {}长链接断开,清理缓存数据",session_id,user_id,(size_t)conn.get());
    }
    void onMessage(websocketpp::connection_hdl hdl, server_t::message_ptr msg){
        auto conn = __ws_server.get_con_from_hdl(hdl);
        chat_im::ClientAuthenticationReq request;
        bool ret = request.ParseFromString(msg->get_payload());
        if(ret == false){
            ERROR("长连接身份识别失败,正文反序列化失败");
            __ws_server.close(hdl,websocketpp::close::status::unsupported_data,"正文反序列化失败");
            return;
        }
        std::string session_id = request.session_id();
        auto user_id = __redis_session_manager->uid(session_id);
        if(!user_id){
            ERROR("长连接身份识别失败,未找到会话信息{}",session_id);
            __ws_server.close(hdl,websocketpp::close::status::unsupported_data,"未找到会话信息");
            return;
        }
        __ws_connection_manager->insert(conn,*user_id,session_id);
        DEBUG("新增长链接管理:{}-{}-{}",session_id,*user_id,(size_t)conn.get());
        keepAlive(conn);

    }
    void keepAlive(server_t::connection_ptr conn) {
        if (!conn || conn->get_state() != websocketpp::session::state::value::open) {
            DEBUG("非正常连接状态，结束连接保活");
            return;
        }
        conn->ping("");
        __ws_server.set_timer(60000, std::bind(&gatewayService::keepAlive, this, conn));
    }

    //用户登录
    void UserLogin(const httplib::Request &request, httplib::Response &response){
        chat_im::UserLoginReq req;
        chat_im::UserLoginRsp rsp;
        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("用户登录反序列化失败");
            return err_response("用户登录反序列化失败");
        }

        //获取用户服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__user_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的用户子服务节点！",req.request_id());
            return err_response("未找到可提供业务处理的用户子服务节点！");
        }

        chat_im::UserService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.UserLogin(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 用户子服务调用失败！", req.request_id());
            return err_response("用户子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }

    //获取手机验证码
    void PhoneVerifyCode(const httplib::Request& request,httplib::Response& response){
        chat_im::PhoneVerifyCodeReq req;
        chat_im::PhoneVerifyCodeRsp rsp;
        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("用户获取手机验证码反序列化失败");
            return err_response("用户获取手机验证码反序列化失败");
        }

        //获取用户服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__user_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的用户子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的用户子服务节点！");
        }

        chat_im::UserService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetPhoneVerifyCode(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 用户子服务调用失败！", req.request_id());
            return err_response("用户子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }    

    //手机号注册
    void PhoneRegister(const httplib::Request& request,httplib::Response& response){
        chat_im::PhoneRegisterReq req;
        chat_im::PhoneRegisterRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("用户手机号注册反序列化失败");
            return err_response("用户手机号注册反序列化失败");
        }

        //获取用户服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__user_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的用户子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的用户子服务节点！");
        }

        chat_im::UserService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.PhoneRegister(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 用户子服务调用失败！", req.request_id());
            return err_response("用户子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }

    //手机号登录
    void PhoneLogin(const httplib::Request& request,httplib::Response& response){
        chat_im::PhoneLoginReq req;
        chat_im::PhoneLoginRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("用户手机号登录反序列化失败");
            return err_response("用户手机号登录反序列化失败");
        }

        //获取用户服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__user_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的用户子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的用户子服务节点！");
        }

        chat_im::UserService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.PhoneLogin(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 用户子服务调用失败！", req.request_id());
            return err_response("用户子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //获取用户信息
    void GetUserInfo(const httplib::Request& request,httplib::Response& response){
        chat_im::GetUserInfoReq req;
        chat_im::GetUserInfoRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("获取用户信息反序列化失败");
            return err_response("获取用户信息反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取用户服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__user_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的用户子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的用户子服务节点！");
        }

        chat_im::UserService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetUserInfo(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 用户子服务调用失败！", req.request_id());
            return err_response("用户子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //设置用户头像
    void SetUserAvatar(const httplib::Request& request,httplib::Response& response){
        chat_im::SetUserAvatarReq req;
        chat_im::SetUserAvatarRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("设置用户头像反序列化失败");
            return err_response("设置用户头像反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取用户服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__user_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的用户子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的用户子服务节点！");
        }

        chat_im::UserService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.SetUserAvatar(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 用户子服务调用失败！", req.request_id());
            return err_response("用户子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }

    //设置用户昵称
    void SetUserNickname(const httplib::Request& request,httplib::Response& response){
        chat_im::SetUserNicknameReq req;
        chat_im::SetUserNicknameRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("设置用户昵称反序列化失败");
            return err_response("设置用户昵称反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取用户服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__user_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的用户子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的用户子服务节点！");
        }

        chat_im::UserService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.SetUserNickname(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 用户子服务调用失败！", req.request_id());
            return err_response("用户子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //设置用户 个性签名
    void SetUserDescription(const httplib::Request& request,httplib::Response& response){
        chat_im::SetUserDescriptionReq req;
        chat_im::SetUserDescriptionRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("设置用户个性签名反序列化失败");
            return err_response("设置用户 个性签名反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取用户服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__user_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的用户子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的用户子服务节点！");
        }

        chat_im::UserService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.SetUserDescription(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 用户子服务调用失败！", req.request_id());
            return err_response("用户子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //设置用户手机号码
    void SetUserPhoneNumber(const httplib::Request& request,httplib::Response& response){
        chat_im::SetUserPhoneNumberReq req;
        chat_im::SetUserPhoneNumberRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("设置用户手机号码反序列化失败");
            return err_response("设置用户手机号码反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取用户服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__user_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的用户子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的用户子服务节点！");
        }

        chat_im::UserService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.SetUserPhoneNumber(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 用户子服务调用失败！", req.request_id());
            return err_response("用户子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //获取好友列表
    void GetFriendList(const httplib::Request& request,httplib::Response& response){
        chat_im::GetFriendListReq req;
        chat_im::GetFriendListRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("获取好友列表反序列化失败");
            return err_response("获取好友列表反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取好友服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__friend_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的好友子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的好友子服务节点！");
        }

        chat_im::FriendService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetFriendList(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }

    //删除好友
    void FriendRemove(const httplib::Request& request,httplib::Response& response){
        chat_im::FriendRemoveReq req;
        chat_im::FriendRemoveRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("删除好友反序列化失败");
            return err_response("删除好友反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取好友服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__friend_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的好友子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的好友子服务节点！");
        }

        chat_im::FriendService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.FriendRemove(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }
        //进行消息转发 对方在线则组织好友删除通知进行事件通知
        server_t::connection_ptr conn = __ws_connection_manager->connection(req.peer_id());
        if(rsp.success() && conn){
            chat_im::NotifyMessage notify;
            notify.set_event_id(req.request_id());
            notify.set_type(chat_im::NotifyType::FRIEND_REMOVE);
            notify.mutable_friend_remove()->set_user_id(*uid);
            conn->send(notify.SerializeAsString(), websocketpp::frame::opcode::value::binary);
        }
        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //添加好友 -- 提交好友申请
    void FriendAdd(const httplib::Request& request,httplib::Response& response){
        chat_im::FriendAddReq req;
        chat_im::FriendAddRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("提交好友申请反序列化失败");
            return err_response("提交好友申请反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取好友服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__friend_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的好友子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的好友子服务节点！");
        }

        chat_im::FriendService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.FriendAdd(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }
        //进行消息转发 对方在线则组织好友删除通知进行事件通知
        server_t::connection_ptr conn = __ws_connection_manager->connection(req.respondent_id());
        if(rsp.success() && conn){
            //获取申请人的用户信息
            chat_im::UserInfo info;
            bool isOk = __getUserInfo(req.request_id(),*uid,info);
            if(!isOk){
                ERROR("获取{}好友申请者用户信息失败",*uid);
                return err_response("获取{}好友申请者用户信息失败");
            }
            chat_im::NotifyMessage notify;
            notify.set_event_id(req.request_id());
            notify.set_type(chat_im::NotifyType::FRIEND_ADD_APPLY);
            notify.mutable_friend_add_apply()->mutable_user_info()->CopyFrom(info);
            conn->send(notify.SerializeAsString(), websocketpp::frame::opcode::value::binary);
        }
        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //处理好友申请
    void FriendAddProcess(const httplib::Request& request,httplib::Response& response){
        chat_im::FriendAddProcessReq req;
        chat_im::FriendAddProcessRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("提交好友申请反序列化失败");
            return err_response("提交好友申请反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取好友服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__friend_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的好友子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的好友子服务节点！");
        }

        chat_im::FriendService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.FriendAddProcess(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }
        //进行消息转发 对方在线则组织好友删除通知进行事件通知
        DEBUG("apply_user_id:{},user_id:{}",req.apply_user_id(),*uid);
        server_t::connection_ptr conn = __ws_connection_manager->connection(req.apply_user_id());
        if(rsp.success() && conn){
            //获取申请人的用户信息
            chat_im::UserInfo info,user_info;//info是被申请者,user_info是申请者
            bool isOk = __getUserInfo(req.request_id(),*uid,info);
            if(!isOk){
                ERROR("获取{}好友申请者用户信息失败",*uid);
                return err_response("获取{}好友申请者用户信息失败");
            }
            //获取被申请者的用户信息
            isOk = __getUserInfo(req.request_id(),req.apply_user_id(),user_info);
            if(!isOk){
                ERROR("获取{}好友申请者用户信息失败",req.apply_user_id());
                return err_response("获取{}好友申请者用户信息失败");
            }
            chat_im::NotifyMessage notify;
            notify.set_event_id(req.request_id());
            notify.set_type(chat_im::NotifyType::FRIEMD_ADD_PROCESS);
            notify.mutable_friend_add_process()->set_agree(req.agree());
            notify.mutable_friend_add_process()->mutable_user_info()->CopyFrom(info);
            conn->send(notify.SerializeAsString(), websocketpp::frame::opcode::value::binary);

            //如果是同意请求的话,额外在发送有新建会话的消息
            if(req.agree() == true){
                //先给申请人发送新建会话
                chat_im::NotifyMessage create_chat_session_notify;
                create_chat_session_notify.set_event_id(req.request_id());
                create_chat_session_notify.set_type(chat_im::NotifyType::CHAT_SESSION_CREATE);

                chat_im::ChatSessionInfo* chat_session = create_chat_session_notify.mutable_new_chat_session_info()->mutable_chat_session_info();
                chat_session->set_single_id(*uid);
                chat_session->set_chat_session_id(rsp.new_session_id());
                chat_session->set_chat_session_name(info.nickname());
                chat_session->set_avatar(info.avatar());
                conn->send(create_chat_session_notify.SerializeAsString(),websocketpp::frame::opcode::binary);
                DEBUG("对申请人{}进行会话创建通知！",req.apply_user_id());
            }
            //获取被申请人的ws链接 ,并发送创建会话的消息
            server_t::connection_ptr user_conn = __ws_connection_manager->connection(*uid);
            if(user_conn){
                //给被申请人发送新建会话
                chat_im::NotifyMessage create_chat_session_notify;
                create_chat_session_notify.set_event_id(req.request_id());
                create_chat_session_notify.set_type(chat_im::NotifyType::CHAT_SESSION_CREATE);
                chat_im::ChatSessionInfo* chat_session = create_chat_session_notify.mutable_new_chat_session_info()->mutable_chat_session_info();
                chat_session->set_single_id(user_info.user_id());
                chat_session->set_chat_session_id(rsp.new_session_id());
                chat_session->set_chat_session_name(user_info.nickname());
                chat_session->set_avatar(user_info.avatar());
                user_conn->send(create_chat_session_notify.SerializeAsString(),websocketpp::frame::opcode::binary);
                DEBUG("对处理人{}进行会话创建通知！",*uid);
            }
        }
        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //获取待处理的好友申请
    void GetPendingFriendEventList(const httplib::Request& request,httplib::Response& response){
        chat_im::GetPendingFriendEventListReq req;
        chat_im::GetPendingFriendEventListRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("获取待处理的好友申请反序列化失败");
            return err_response("获取待处理的好友申请反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取好友服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__friend_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的好友子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的好友子服务节点！");
        }

        chat_im::FriendService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetPendingFriendEventList(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //好友搜索 -- 搜索用户
    void FriendSearch(const httplib::Request& request,httplib::Response& response){
        chat_im::FriendSearchReq req;
        chat_im::FriendSearchRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("好友搜索 -- 搜索用户反序列化失败");
            return err_response("好友搜索 -- 搜索用户反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取好友服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__friend_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的好友子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的好友子服务节点！");
        }

        chat_im::FriendService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.FriendSearch(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //获取用户聊天会话列表
    void GetChatSessionList(const httplib::Request& request,httplib::Response& response){
        chat_im::GetChatSessionListReq req;
        chat_im::GetChatSessionListRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("获取用户聊天会话列表申请反序列化失败");
            return err_response("获取用户聊天会话列表反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取好友服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__friend_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的好友子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的好友子服务节点！");
        }

        chat_im::FriendService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetChatSessionList(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //创建多人聊天会话
    void ChatSessionCreate(const httplib::Request& request,httplib::Response& response){
        chat_im::ChatSessionCreateReq req;
        chat_im::ChatSessionCreateRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("创建多人聊天会话申请反序列化失败");
            return err_response("创建多人聊天会话反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取好友服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__friend_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的好友子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的好友子服务节点！");
        }

        chat_im::FriendService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.ChatSessionCreate(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }

        //如果创建成功则向会话成员进行转发创建群聊消息
        if(rsp.success()){
            for (auto i = req.mutable_member_id_list()->begin(); i != req.mutable_member_id_list()->end(); ++i)
            {
                server_t::connection_ptr conn = __ws_connection_manager->connection(*i);
                if (conn)
                {
                    chat_im::NotifyMessage notify;
                    notify.set_event_id(req.request_id());
                    notify.set_type(chat_im::NotifyType::CHAT_SESSION_CREATE);
                    notify.mutable_new_chat_session_info()->mutable_chat_session_info()->CopyFrom(rsp.chat_session_info());
                    conn->send(notify.SerializeAsString(), websocketpp::frame::opcode::value::binary);
                    DEBUG("对群聊成员 {} 进行会话创建通知", *i);
                }
            }
        }
        rsp.clear_chat_session_info();

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf"); 
    }
    
    //获取消息会话成员列表
    void GetChatSessionMember(const httplib::Request& request,httplib::Response& response){
        chat_im::GetChatSessionMemberReq req;
        chat_im::GetChatSessionMemberRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("获取消息会话成员列表反序列化失败");
            return err_response("获取消息会话成员列表反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取好友服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__friend_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的好友子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的好友子服务节点！");
        }

        chat_im::FriendService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetChatSessionMember(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //获取历史聊天记录
    void GetHistoryMsg(const httplib::Request& request,httplib::Response& response){
        chat_im::GetHistoryMsgReq req;
        chat_im::GetHistoryMsgRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("获取历史聊天记录反序列化失败");
            return err_response("获取历史聊天记录反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取消息存储服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__message_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的消息存储子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的消息存储子服务节点！");
        }

        chat_im::MsgStorageService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetHistoryMsg(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //获取最近聊天消息记录
    void GetRecentMsg(const httplib::Request& request,httplib::Response& response){
        chat_im::GetRecentMsgReq req;
        chat_im::GetRecentMsgRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("获取最近聊天消息记录反序列化失败");
            return err_response("获取最近聊天消息记录反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取消息存储服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__message_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的消息存储子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的消息存储子服务节点！");
        }

        chat_im::MsgStorageService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetRecentMsg(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //关键词搜索历史消息
    void MsgSearch(const httplib::Request& request,httplib::Response& response){
        chat_im::MsgSearchReq req;
        chat_im::MsgSearchRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("关键词搜索历史消息反序列化失败");
            return err_response("关键词搜索历史消息反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取消息存储服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__message_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的消息存储子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的消息存储子服务节点！");
        }

        chat_im::MsgStorageService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.MsgSearch(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 好友子服务调用失败！", req.request_id());
            return err_response("好友子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }

    //发送新消息
    void NewMessage(const httplib::Request& request,httplib::Response& response){
        chat_im::NewMessageReq req;
        chat_im::GetTransmitTargetRsp transmit_rsp;
        chat_im::NewMessageRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("发送新消息反序列化失败");
            return err_response("发送新消息反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取消息存储服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__transmit_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的消息转发子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的消息转发子服务节点！");
        }
        //处理成功
        DEBUG("获取消息存储服务成功");

        chat_im::MsgTransmitService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetTransmitTarget(&cntl,&req,&transmit_rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 消息转发子服务调用失败！", req.request_id());
            return err_response(" 消息转发子服务调用失败！");
        }
        DEBUG("调用消息转发子服务成功");
        //处理成功
        if(transmit_rsp.success()){
            chat_im::NotifyMessage message;
            message.set_event_id(req.request_id());
            message.set_type(chat_im::NotifyType::CHAT_MESSAGE);
            message.mutable_new_message_info()->mutable_message_info()->CopyFrom(transmit_rsp.message());
            for(auto i = transmit_rsp.target_id_list().begin();i!=transmit_rsp.target_id_list().end();++i){
                //不通知自己
                if(i->compare(*uid) == 0){
                    continue;
                }
                server_t::connection_ptr conn = __ws_connection_manager->connection(*i);
                //若长连接存在
                if(conn){
                    conn->send(message.SerializeAsString(),websocketpp::frame::opcode::value::binary);
                    DEBUG("向{}转发提醒有新消息",*i);
                }
            }
        }
        rsp.set_request_id(transmit_rsp.request_id());
        rsp.set_success(transmit_rsp.success());
        rsp.set_errmsg(transmit_rsp.errmsg());

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");

    }

    //获取文件
    void GetSingleFile(const httplib::Request& request,httplib::Response& response){
        chat_im::GetSingleFileReq req;
        chat_im::GetSingleFileRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("获取文件请求反序列化失败");
            return err_response("获取文件反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取消息存储服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__file_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的文件子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的文件子服务节点！");
        }

        chat_im::FileService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetSingleFile(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 文件子服务调用失败！", req.request_id());
            return err_response("文件子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //获取多个文件
    void GetMultiFile(const httplib::Request& request,httplib::Response& response){
        chat_im::GetMultiFileReq req;
        chat_im::GetMultiFileRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("获取多个文件请求反序列化失败");
            return err_response("获取多个文件反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取消息存储服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__file_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的文件子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的文件子服务节点！");
        }

        chat_im::FileService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetMultiFile(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 文件子服务调用失败！", req.request_id());
            return err_response("文件子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }

    //上传单个文件
    void PutSingleFile(const httplib::Request& request,httplib::Response& response){
        chat_im::PutSingleFileReq req;
        chat_im::PutSingleFileRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("上传单个文件反序列化失败");
            return err_response("上传单个文件反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取消息存储服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__file_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的文件子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的文件子服务节点！");
        }

        chat_im::FileService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.PutSingleFile(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 文件子服务调用失败！", req.request_id());
            return err_response("文件子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }
    
    //上传多个文件
    void PutMultiFile(const httplib::Request& request,httplib::Response& response){
        chat_im::PutMultiFileReq req;
        chat_im::PutMultiFileRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("上传多个文件反序列化失败");
            return err_response("上传多个文件反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取消息存储服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__file_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的文件子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的文件子服务节点！");
        }

        chat_im::FileService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.PutMultiFile(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 文件子服务调用失败！", req.request_id());
            return err_response("文件子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }

    //语音服务
    void SpeechRecognition(const httplib::Request& request,httplib::Response& response){
        chat_im::SpeechRecognitionReq req;
        chat_im::SpeechRecognitionRsp rsp;

        auto err_response = [&rsp, &response](const std::string &errmsg) -> void {
            rsp.set_success(false);
            rsp.set_errmsg(errmsg);
            response.set_content(rsp.SerializeAsString(), "application/x-protbuf");
        };
        bool isOk = req.ParseFromString(request.body);
        if(!isOk){
            ERROR("语音服务反序列化失败");
            return err_response("上语音服务反序列化失败");
        }
        //获取对应会话的用户id
        sw::redis::OptionalString uid =  __redis_session_manager->uid(req.session_id());
        //redis中为获取到会话有对应的用户id
        if(!uid){
            ERROR("{} 获取登录会话关联用户信息失败",req.session_id());
            return err_response("获取登录会话关联用户信息失败！");
        }
        req.set_user_id(*uid);

        //获取消息存储服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__speech_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的语音子服务节点！",req.request_id());
            return ERROR("未找到可提供业务处理的语音子服务节点！");
        }

        chat_im::SpeechService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.SpeechRecognition(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 语音子服务调用失败！", req.request_id());
            return err_response("语音子服务调用失败！");
        }

        response.set_content(rsp.SerializeAsString(),"application/x-protbuf");
    }

private:

    //单独获取用户信息
    bool __getUserInfo(const std::string& request_id,const std::string& user_id,chat_im::UserInfo& info){
        chat_im::GetUserInfoReq req;
        chat_im::GetUserInfoRsp rsp;
        req.set_request_id(request_id);
        req.set_user_id(user_id);
        
        //获取用户服务
        util::ServiceChannel::ChannelPtr user_channel =  __channel_manager->choose(__user_service_name);
        if(!user_channel){
            ERROR("{}:未找到可提供业务处理的用户子服务节点！",req.request_id());
            return false;
        }

        chat_im::UserService_Stub stub(user_channel.get());
        brpc::Controller cntl;
        stub.GetUserInfo(&cntl,&req,&rsp,nullptr);

        if (cntl.Failed()) {
            ERROR("{} 用户子服务调用失败！", req.request_id());
            return false;
        }
        if(rsp.success()){
            info.CopyFrom(rsp.user_info());
        }
        return true;
        
    }


private:
    server_t __ws_server;
    httplib::Server __http_server;
    std::thread __http_thread;

    util::ServiceChannelManager::ptr __channel_manager;
    util::Session::ptr __redis_session_manager;
    util::Status::ptr __redis_status_manager;
    WSConnection::ptr __ws_connection_manager;
    util::Discovery::ptr __service_discovery;

    std::string __user_service_name;
    std::string __friend_service_name;
    std::string __message_service_name;
    std::string __file_service_name;
    std::string __speech_service_name;
    std::string __transmit_service_name;
};

class gatewayServiceBuilder{
public:
     //构造redis客户端对象
     void make_redis_object(const std::string &host,
        int port,
        int db,
        bool keep_alive) {
        __redis_client = util::RedisClientFactory::create(host, port, db, keep_alive);
    }
    //构建ServiceChannel管理对象,关注文件服务,同时去寻找服务
    void makeChannelManager(
        const std::string &etcd_host,
        const std::string &base_service,
        const std::string& file_service_name,
        const std::string& user_service_name,
        const std::string& speech_service_name,
        const std::string& message_service_name,
        const std::string& friend_service_name,
        const std::string& transmite_service_name)
    {
        __file_service_name =file_service_name;
        __user_service_name = user_service_name;
        __speech_service_name =speech_service_name;
        __message_service_name =message_service_name;
        __friend_service_name = friend_service_name;
        __transmite_service_name = transmite_service_name;
        __channel_manager = std::make_shared<chat_im::util::ServiceChannelManager>();
        __channel_manager->declared(__file_service_name);
        __channel_manager->declared(__user_service_name);
        __channel_manager->declared(__speech_service_name);
        __channel_manager->declared(__message_service_name);
        __channel_manager->declared(__friend_service_name);
        __channel_manager->declared(__transmite_service_name);
  
        std::function<void(const std::string&,const std::string&)>    \
          put_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOnline,__channel_manager.get(),std::placeholders::_1,std::placeholders::_2);
        std::function<void(const std::string&,const std::string&)>    \
          del_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOffline,__channel_manager.get(),std::placeholders::_1,std::placeholders::_2);
        
        __discovery = std::make_shared<chat_im::util::Discovery>(etcd_host,base_service,put_cb,del_cb);
    }
    void make_server_object(int websocket_port, int http_port) {
        __websocket_port = websocket_port;
        __http_port = http_port;
    }
    //构造RPC服务器对象
    gatewayService::ptr build() {
        if (!__redis_client) {
            ERROR("还未初始化Redis客户端模块！");
            abort();
        }
        if (!__discovery) {
            ERROR("还未初始化服务发现模块！");
            abort();
        }
        if (!__channel_manager) {
            ERROR("还未初始化信道管理模块！");
            abort();
        }
        gatewayService::ptr server = std::make_shared<gatewayService>(
            __websocket_port, __http_port, __redis_client, __channel_manager, 
            __discovery, __user_service_name, __file_service_name,
            __speech_service_name, __message_service_name, 
            __transmite_service_name, __friend_service_name);
        return server;
    } 

private:
    int __websocket_port;
    int __http_port;

    std::shared_ptr<brpc::Server> __message_service;
   std::string __file_service_name;
   std::string __user_service_name;
   std::string __speech_service_name;
   std::string __message_service_name;
   std::string __friend_service_name;
   std::string __transmite_service_name;
   
   std::shared_ptr<sw::redis::Redis> __redis_client;
   chat_im::util::ServiceChannelManager::ptr __channel_manager;
   chat_im::util::Discovery::ptr __discovery;
   chat_im::util::MQClient::ptr __mq_client;
};