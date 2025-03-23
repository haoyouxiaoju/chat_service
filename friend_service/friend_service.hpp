#ifndef FRIEND_SERVICE_HPP
#define FRIEND_SERVICE_HPP

#include <brpc/server.h>
#include <butil/logging.h>

#include "friend.pb.h"
#include "base.pb.h"
#include "message.pb.h"
#include "user.pb.h"

#include "mysql/sql_chat_session.hpp"
#include "mysql/sql_chat_session_member.hpp"
#include "mysql/sql_friend_event.hpp"
#include "mysql/sql_friend_ralation.hpp"
#include "data_es.hpp"

#include "etcd.hpp"
#include "channel.hpp"
#include "utils.hpp"

namespace PROTOBUF_NAMESPACE_ID = google::protobuf;

class FriendServiceImpl : public chat_im::FriendService
{
public:
        FriendServiceImpl(
                const std::shared_ptr<odb::core::database>& db,
                const std::shared_ptr<elasticlient::Client>& es_client,
                const chat_im::util::ServiceChannelManager::ptr channel_manager,
                const std::string& user_service_name,
                const std::string& message_service_name
        ):      __chat_session_manager(std::make_shared<chat_im::util::ChatSessionTable>(db)),
                __chat_session_member_manager(std::make_shared<chat_im::util::ChatSessionMemberTable>(db)),
                __friend_event_manager(std::make_shared<chat_im::util::FriendEventTable>(db)),
                __friend_ralation_manager(std::make_shared<chat_im::util::FriendRalationTable>(db)),
                __es_user_manager(std::make_shared<chat_im::util::ESUser>(es_client)),__channel_manager(channel_manager),
                __user_service_name(user_service_name),
                __message_service_name(message_service_name){}
        ~FriendServiceImpl(){}

        //获取好友列表的用户信息
    void GetFriendList(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                       const ::chat_im::GetFriendListReq *request,
                       ::chat_im::GetFriendListRsp *response,
                       ::google::protobuf::Closure *done)
    {
        brpc::ClosureGuard guard(done);

        const std::string request_id = request->request_id();
        const std::string user_id = request->user_id();

        std::function<void(const std::string&)> err_response=
                [response](const std::string& err_msg)->void{
                        response->set_errmsg(err_msg);
                        response->set_success(false);
                        return ;
                };

        //设置request_id
        response->set_request_id(request_id);

        //获取friendId 
        std::vector<std::string> friend_list (__friend_ralation_manager->select_friend(user_id));

        std::unordered_set<std::string> friend_id_set;
        for (auto i = friend_list.begin(); i != friend_list.end(); ++i)
        {
                friend_id_set.insert(i->c_str());
        }

        //从用户服务模块请求获取好友列表的用户信息
        std::unordered_map<std::string,chat_im::UserInfo> user_info_list;
        bool isOk = __getMultiUserInfo(request_id,friend_id_set,user_info_list); 
        if(!isOk){
                ERROR("获取好友列表的用户信息失败");
                return err_response("获取好友列表的用户信息失败");
        }

        for(auto& [id,info] :user_info_list){
               response->add_friend_list()->CopyFrom(info); 
        }
        //成功构建
        response->set_success(true);
    }

        //删除好友关系
    void FriendRemove(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                      const ::chat_im::FriendRemoveReq *request,
                      ::chat_im::FriendRemoveRsp *response,
                      ::google::protobuf::Closure *done){
                        brpc::ClosureGuard guard(done);
                        
                        //获取请求中的元素
                        const std::string request_id = request->request_id();
                        const std::string user_id = request->user_id();
                        const std::string friend_id = request->peer_id();

                        std::function<void(const std::string &)> err_response =
                            [response](const std::string &err_msg) -> void
                        {
                                response->set_errmsg(err_msg);
                                response->set_success(false);
                                return;
                        };

                        //从数据库删除好友关系
                        bool isOk = __friend_ralation_manager->remove(user_id,friend_id);
                        if(!isOk){
                                ERROR("删除{}-{}好友关系失败",user_id,friend_id);
                                return err_response("删除好友关系失败");
                        }
                        response->set_success(true);
                      }


        //添加好友申请记录 --defalut 待处理
    void FriendAdd(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                   const ::chat_im::FriendAddReq *request,
                   ::chat_im::FriendAddRsp *response,
                   ::google::protobuf::Closure *done){
                        brpc::ClosureGuard guard(done);

                        std::function<void(const std::string &)> err_response =
                            [response](const std::string &err_msg) -> void
                        {
                                response->set_errmsg(err_msg);
                                response->set_success(false);
                                return;
                        };

                        //获取请求中的元素
                        const std::string request_id = request->request_id();
                        const std::string user_id = request->user_id();
                        const std::string respondent_id = request->respondent_id(); //被添加人
                        DEBUG("申请者:{},被申请者:{}",user_id,respondent_id);

                        // 设置request_id
                        response->set_request_id(request_id);

                        //数据库插入好友事件申请记录
                        std::string uuid = chat_im::util::uuid();
                        chat_im::friendEvent event(uuid,user_id,respondent_id);
                        bool isOk = __friend_event_manager->insert(event);
                        if(!isOk){
                                ERROR("{}请求{}好友申请失败",user_id,respondent_id);
                                return err_response("请求好友申请失败");
                        }

                        response->set_notify_event_id(uuid);
                        response->set_success(true);
                   }

        // 处理好友申请 --      
    void FriendAddProcess(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                          const ::chat_im::FriendAddProcessReq *request,
                          ::chat_im::FriendAddProcessRsp *response,
                          ::google::protobuf::Closure *done){

                        brpc::ClosureGuard guard(done);

                        std::function<void(const std::string &)> err_response =
                            [response](const std::string &err_msg) -> void
                        {
                                response->set_errmsg(err_msg);
                                response->set_success(false);
                                return;
                        };

                        //获取请求中的元素
                        const std::string request_id = request->request_id();
                        const std::string user_id = request->user_id();    //被添加人     
                        const std::string notify_event_id = request->notify_event_id(); //通知事件id    
                        const std::string apply_user_id = request->apply_user_id();     //申请人 id
                        const bool agree = request->agree();

                        //判断是否有好友申请事件
                        std::shared_ptr<chat_im::friendEvent> event 
                                = __friend_event_manager->select_userIdAndFriendId(user_id,apply_user_id);
                        //没有找到申请事件
                        if(!event){
                                ERROR("没有查询到{}-{}的好友申请记录",user_id,apply_user_id);
                                return err_response("没有查询到此好友申请记录");
                        }
                        bool isOk;
                        //先处理 是否同意 同意创建会话信息等
                        if(agree){
                                //此insert中是插入俩次记录,即 user_id->friend_id和friend_id->user_id
                                chat_im::friendRelation relation(user_id,apply_user_id);
                                isOk = __friend_ralation_manager->insert(relation);
                                if(!isOk){
                                     ERROR("创建{}_{}的好友关系失败",user_id,apply_user_id);
                                     return err_response("创建的好友关系失败");           
                                }
                                std::string chat_session_id = chat_im::util::uuid();
                                //插入新的会话信息 -- 好友会话
                                chat_im::chatSession session(chat_session_id,"",chat_im::chatSessionType::SINGLE);
                                isOk = __chat_session_manager->insert(session);
                                if(!isOk){
                                     ERROR("创建{}_{}的会话信息失败",user_id,apply_user_id)
                                     return err_response("创建的会话信息失败");           
                                }
                                
                                //插入俩条新的会话成员信息 -- user_id and friend_id
                                ;
                                isOk =__chat_session_member_manager->insert(chat_session_id,std::vector<std::string>({user_id,apply_user_id}));
                                if(!isOk){
                                     ERROR("创建{}_{}的会话成员消息失败",user_id,apply_user_id)
                                     return err_response("创建的会话成员消息失败");
                                }
                                event->event_status(chat_im::friendStatus::ACCEPT);
                                isOk = __friend_event_manager->update(event);
                                if (!isOk)
                                {
                                        ERROR("修改事件状态失败");
                                        return err_response("修改事件状态失败");
                                }

                                response->set_success(true);
                                response->set_new_session_id(chat_session_id);
                                return ;
                                
                        }
                        //不同意
                        //需求文档要求是会删除请求记录,但我建议不删除
                        //      暂时采取修改事件状态,不删除事件
                        //
                        event->event_status(chat_im::friendStatus::REJECT);
                        isOk = __friend_event_manager->update(event);
                        if(!isOk){
                                ERROR("修改事件状态失败");
                                return err_response("修改事件状态失败");
                        }
                        response->set_success(true);

                        }

        // 搜索用户(除好友外的关键词搜索),非搜索好友
    void FriendSearch(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                      const ::chat_im::FriendSearchReq *request,
                      ::chat_im::FriendSearchRsp *response,
                      ::google::protobuf::Closure *done)
                      {
                        brpc::ClosureGuard guard(done);

                        std::function<void(const std::string &)> err_response =
                            [response](const std::string &err_msg) -> void
                        {
                                response->set_errmsg(err_msg);
                                response->set_success(false);
                                return;
                        };

                        const std::string request_id = request->request_id();
                        const std::string user_id = request->user_id();
                        const std::string search_key = request->search_key();
                        //
                        response->set_request_id(request_id);

                        // 获取friendId
                        std::vector<std::string> friend_list(__friend_ralation_manager->select_friend(user_id));
                        friend_list.push_back(user_id);

                        //从es服务器中进行用户搜索,为用户 ID/手机号/昵称的搜索关键字进行搜索
                        std::vector<chat_im::User> user_list = __es_user_manager->search(search_key, friend_list);
                        
                        
                        //获取用户id,从用户服务模块获取用户数据
                        std::unordered_set<std::string> user_set;
                        for(auto i = user_list.begin();i!=user_list.end();++i){                               
                                user_set.insert((*i).user_id());
                        }        
                        //由于用户模块就存在了多用户查询并返回包括头像数据的请求,所以不考虑直接向文件服务模块获取头像数据
                        //直接向用户模块获取,减少代码编写,同时提高使用(在获取好友列表就有使用到)
                        std::unordered_map<std::string,chat_im::UserInfo> user_info_list;
                        bool isOk = __getMultiUserInfo(request_id,user_set,user_info_list);
                        if(!isOk){
                                ERROR("获取搜索到的用户消息失败");
                                return err_response("获取搜索到的用户消息失败");
                        }

                        //组装
                        for(auto&[id,info]:user_info_list){
                                response->add_user_info()->CopyFrom(info);
                        }
                        response->set_success(true);
                                    
                }


                //获取聊天会话列表
    void GetChatSessionList(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                            const ::chat_im::GetChatSessionListReq *request,
                            ::chat_im::GetChatSessionListRsp *response,
                            ::google::protobuf::Closure *done){
                                brpc::ClosureGuard guard(done);
                        std::function<void(const std::string &)> err_response =
                            [response](const std::string &err_msg) -> void
                        {
                                response->set_errmsg(err_msg);
                                response->set_success(false);
                                return;
                        };

                        const std::string request_id = request->request_id();
                        const std::string user_id = request->user_id();
                        //
                        response->set_request_id(request_id);

                        // //获取会话信息
                        // std::unordered_map<std::string,chat_im::ChatSessionInfo> ret_data;
                        //单聊要获取好友信息
                        std::unordered_set<std::string> friend_id_list;
                        std::vector<chat_im::SingleChatSession> singleChatSession_list           \
                                = __chat_session_manager->singleChatSession(user_id);
                        for(const chat_im::SingleChatSession& elem : singleChatSession_list){
                                std::string id = elem.friend_id;
                                friend_id_list.insert(id);
                        }
                        std::unordered_map<std::string,chat_im::UserInfo> user_info_list;
                        bool isOk = __getMultiUserInfo(request_id,friend_id_list,user_info_list);
                        if(!isOk){
                                ERROR("获取搜索到的用户消息失败");
                                return err_response("获取搜索到的用户消息失败");
                        }

                        //从数据库中查询出用户的群聊会话列表
                        std::vector<chat_im::GroupChatSession> groupChatSession_list \
                                = __chat_session_manager->groupChatSession(user_id);
                        //单聊会话的构建
                        for (const chat_im::SingleChatSession &elem : singleChatSession_list)
                        {
                                chat_im::ChatSessionInfo* info = response->add_chat_session_info_list();
                                info->set_single_id(elem.friend_id);
                                info->set_chat_session_id(elem.chat_session_id);
                                info->set_chat_session_name(user_info_list[elem.friend_id].nickname());
                                info->set_avatar(user_info_list[elem.friend_id].avatar());
                                chat_im::MessageInfo  m_info;
                                isOk = __GetRecentMsg(request_id,elem.chat_session_id,m_info);
                                if(!isOk){
                                        continue;
                                }
                                info->mutable_prev_message()->CopyFrom(m_info);
                        }
                        //群聊会话的构建
                        for(const chat_im::GroupChatSession &elem:groupChatSession_list){
                                chat_im::ChatSessionInfo *info = response->add_chat_session_info_list();
                                info->set_chat_session_id(elem.chat_session_id);
                                info->set_chat_session_name(elem.chat_session_name);
                                chat_im::MessageInfo  m_info;
                                isOk = __GetRecentMsg(request_id,elem.chat_session_id,m_info);
                                if(!isOk){
                                        continue;
                                }
                                info->mutable_prev_message()->CopyFrom(m_info);
                        }
                        
                        response->set_success(true);

                        }

                //群聊的创建
    void ChatSessionCreate(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                           const ::chat_im::ChatSessionCreateReq *request,
                           ::chat_im::ChatSessionCreateRsp *response,
                           ::google::protobuf::Closure *done){
                                brpc::ClosureGuard guard(done);
                        std::function<void(const std::string &)> err_response =
                            [response](const std::string &err_msg) -> void
                        {
                                response->set_errmsg(err_msg);
                                response->set_success(false);
                                return;
                        };

                        const std::string request_id = request->request_id();
                        const std::string user_id = request->user_id();
                        const std::string chat_session_name = request->chat_session_name();
                        //
                        response->set_request_id(request_id);
                        //添加会话信息
                        const std::string chat_session_id = chat_im::util::uuid();
                        chat_im::chatSession c_session(chat_session_id,chat_session_name,chat_im::chatSessionType::GROUP);
                        bool isOk = __chat_session_manager->insert(c_session);
                        if(!isOk){
                                ERROR("{}向数据库添加会话信息失败 {}", request_id, chat_session_name);
                                return err_response("向数据库添加会话信息失败");
                        }
                        //添加会话成员信息
                        std::vector<std::string> member_id_list;
                        member_id_list.reserve(request->member_id_list_size()+1);
                        member_id_list.push_back(user_id);
                        for(const auto& elem:request->member_id_list()){
                                member_id_list.push_back(elem);
                        }
                        isOk = __chat_session_member_manager->insert(chat_session_id,member_id_list);
                        if(!isOk){
                                ERROR("{}向数据库添加会话成员信息失败 {}", request_id, chat_session_name);
                                return err_response("向数据库添加会话成员信息失败");
                        }
                        //组织响应
                        response->set_success(true);
                        response->mutable_chat_session_info()->set_chat_session_id(chat_session_id);
                        response->mutable_chat_session_info()->set_chat_session_name(chat_session_name);
                        }

                        //获取会话的成员列表
    void GetChatSessionMember(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                              const ::chat_im::GetChatSessionMemberReq *request,
                              ::chat_im::GetChatSessionMemberRsp *response,
                              ::google::protobuf::Closure *done){

                                brpc::ClosureGuard guard(done);
                                std::function<void(const std::string &)> err_response =
                                    [response](const std::string &err_msg) -> void
                                {
                                        response->set_errmsg(err_msg);
                                        response->set_success(false);
                                        return;
                                };
        
                                const std::string request_id = request->request_id();
                                const std::string user_id = request->user_id();
                                const std::string chat_session_id = request->chat_session_id();
                                //
                                response->set_request_id(request_id);
                                //添加会话成员的用户id
                                std::vector<chat_im::chatSessionMember> sessionmember_list    \
                                        = __chat_session_member_manager->select_chat_session_id(chat_session_id);
                                std::unordered_set<std::string> user_id_list;
                                for(auto i = sessionmember_list.begin();i!=sessionmember_list.end();++i){
                                        user_id_list.insert(i->user_id());
                                }
                                // 剔除请求的用户id
                                // user_id_list.erase(user_id);
                                //获取会话成员的信息
                                std::unordered_map<std::string, chat_im::UserInfo> user_info_list;
                                bool isOk = __getMultiUserInfo(request_id,user_id_list,user_info_list);
                                if(!isOk){
                                        ERROR("{}从用户子服务获取用户信息失败!", request_id);
                                        return err_response("从用户子服务获取用户信息失败!");
                                }

                                for(const auto& [id,info]:user_info_list){
                                        response->add_member_info_list()->CopyFrom(info);
                                }
                                response->set_success(true);


                              }

                              //获取待处理的好友申请
    void GetPendingFriendEventList(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                                   const ::chat_im::GetPendingFriendEventListReq *request,
                                   ::chat_im::GetPendingFriendEventListRsp *response,
                                   ::google::protobuf::Closure *done){
                                        brpc::ClosureGuard guard(done);
                                        std::function<void(const std::string &)> err_response =
                                            [response](const std::string &err_msg) -> void
                                        {
                                                response->set_errmsg(err_msg);
                                                response->set_success(false);
                                                return;
                                        };
                
                                        const std::string request_id = request->request_id();
                                        const std::string user_id = request->user_id();
                                        //
                                        response->set_request_id(request_id);
                                        // 获取请求者 用户id
                                        std::vector<chat_im::friendEvent> event_list     \
                                                = __friend_event_manager->select_receiverId(user_id);
                                        std::unordered_set<std::string> sender_id_list;
                                        for(auto i = event_list.begin();i!=event_list.end();++i){
                                                sender_id_list.insert(i->sender_id());
                                        }
                                        //获取请求者成员的信息
                                        std::unordered_map<std::string, chat_im::UserInfo> user_info_list;
                                        bool isOk = __getMultiUserInfo(request_id,sender_id_list,user_info_list);
                                        if(!isOk){
                                                ERROR("{}从用户子服务获取用户信息失败!", request_id);
                                                return err_response("从用户子服务获取用户信息失败!");
                                        }
                                        for(auto i = event_list.begin();i!=event_list.end();++i){
                                               chat_im::FriendEvent* e = response->add_event();
                                               e->set_event_id(i->friend_event_id());
                                               e->mutable_sender()->CopyFrom(user_info_list[i->sender_id()]);

                                        }
                                        response->set_success(true);
                                   }
private:
        bool __GetRecentMsg(const std::string &rid,
                          const std::string &cssid, chat_im::MessageInfo &msg)
        {
                chat_im::util::ServiceChannel::ChannelPtr channel       
                        = __channel_manager->choose(__message_service_name);
                if (!channel)
                {
                        ERROR("{} - 获取消息子服务信道失败！！", rid);
                        return false;
                }
                chat_im::GetRecentMsgReq req;
                chat_im::GetRecentMsgRsp rsp;
                req.set_request_id(rid);
                req.set_chat_session_id(cssid);
                req.set_msg_count(1);
                brpc::Controller cntl;
                chat_im::MsgStorageService_Stub stub(channel.get());
                stub.GetRecentMsg(&cntl, &req, &rsp, nullptr);
                if (cntl.Failed() == true || rsp.success() == false)
                {
                        ERROR("{} - 消息存储子服务调用失败: {}", rid, cntl.ErrorText());
                        return false;
                }
                if (rsp.message_list_size() > 0)
                {
                        msg.CopyFrom(rsp.message_list(0));
                        return true;
                }
                return false;
        }

        bool __getMultiUserInfo(
            const std::string &request_id,
            const std::unordered_set<std::string> &user_id_list,
            std::unordered_map<std::string, chat_im::UserInfo> &user_info_list)
        {
                chat_im::util::ServiceChannel::ChannelPtr channel = __channel_manager->choose(__user_service_name);
                if (!channel)
                {
                        ERROR("未找到用户管理服务模块");
                        return false;
                }

                chat_im::GetMultiUserInfoReq req;
                req.set_request_id(request_id);
                for (const std::string &user_id : user_id_list)
                {
                        req.add_users_id(user_id);
                }

                chat_im::UserService_Stub stub(channel.get());
                brpc::Controller cntl;
                chat_im::GetMultiUserInfoRsp rsp;
                stub.GetMultiUserInfo(&cntl, &req, &rsp, nullptr);

                // 请求失败
                if (cntl.Failed() || !rsp.success())
                {
                        ERROR("用户消息获取失败,失败原因:{}", rsp.errmsg());
                        return false;
                }

                for (const auto &[id, info] : rsp.users_info())
                {
                        user_info_list.insert(std::pair(id, info));
                }


                return true;
        }

       
private:
        chat_im::util::ChatSessionTable::ptr __chat_session_manager;
        chat_im::util::ChatSessionMemberTable::ptr __chat_session_member_manager;
        chat_im::util::FriendEventTable::ptr __friend_event_manager;
        chat_im::util::FriendRalationTable::ptr __friend_ralation_manager;
        chat_im::util::ESUser::ptr __es_user_manager;

        chat_im::util::ServiceChannelManager::ptr __channel_manager;

        std::string __user_service_name;
        std::string __message_service_name;
};

class FriendService
{
public:
   using ptr = std::shared_ptr<FriendService>;
        FriendService(
            const std::shared_ptr<brpc::Server>&friend_service,
            const chat_im::util::Registry::ptr& reg_client,
            const chat_im::util::Discovery::ptr&discovery
        ):__reg_client(reg_client),__discovery(discovery),__friend_service(friend_service)
             {}
        ~FriendService(){}

        void start(){
                __friend_service->RunUntilAskedToQuit();
        }
private:
        chat_im::util::Registry::ptr __reg_client;
        chat_im::util::Discovery::ptr __discovery;
        std::shared_ptr<brpc::Server> __friend_service;
};

class FriendServiceBuilder
{
public:
        void make_regClient(const std::string &reg_host,
                            const std::string &service_name,
                            const std::string &access_host)
        {
                __reg_client = std::make_shared<chat_im::util::Registry>(reg_host);
                __reg_client->registry(service_name, access_host);
        }

        // 构建ServiceChannel管理对象,关注文件服务,同时去寻找服务
        void makeChannelManager(
            const std::string &etcd_host,
            const std::string &base_service,
            const std::string &message_service_name,
            const std::string &user_service_name)
        {
                __message_service_name = message_service_name;
                __user_service_name = user_service_name;
                __channel_manager = std::make_shared<chat_im::util::ServiceChannelManager>();
                __channel_manager->declared(__message_service_name);
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

        // 创建es服务器连接,构建es服务用户管理对象
        void makeESUserManager(const std::vector<std::string> &es_host)
        {
                __es_client = chat_im::util::ESClientFactory::create(es_host);
        }

        void make_friend_service(uint16_t port, int32_t timeout, uint8_t num_threads)
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
                if (!__es_client)
                {
                        ERROR("es服务管理对象未构建");
                        abort();
                }
                

                __friend_service = std::make_shared<brpc::Server>();

                FriendServiceImpl *impl = new FriendServiceImpl(
                    __db,__es_client,__channel_manager,__user_service_name,__message_service_name
                );

                bool isOk = __friend_service->AddService(impl, brpc::ServiceOwnership::SERVER_OWNS_SERVICE);
                if (isOk == -1)
                {
                        ERROR("rpc服务添加失败");
                        abort();
                }

                brpc::ServerOptions opt;
                opt.idle_timeout_sec = timeout;
                opt.num_threads = num_threads;
                isOk = __friend_service->Start(port, &opt);
                if (isOk == -1)
                {
                        ERROR("rpc服务启动失败");
                        abort();
                }

        }

        FriendService::ptr build(){
                if (!__discovery)
                {
                    ERROR("未初始化服务发现模块！");
                    abort();
                }
                if (!__friend_service)
                {
                    ERROR("未初始化rpc服务器模块");
                    abort();
                }
                if (!__reg_client)
                {
                    ERROR("未初始化服务注册模块");
                    abort();
                }
        
                return std::make_shared<FriendService>(__friend_service,__reg_client,__discovery);
        
            }

private:

        chat_im::util::Registry::ptr __reg_client;
        chat_im::util::Discovery::ptr __discovery;
        chat_im::util::ServiceChannelManager::ptr __channel_manager;
        std::shared_ptr<brpc::Server> __friend_service;
        std::shared_ptr<odb::core::database> __db;
        std::shared_ptr<elasticlient::Client> __es_client;
        std::string __user_service_name;
        std::string __message_service_name;
};

#endif 

