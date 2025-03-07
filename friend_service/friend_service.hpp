#ifndef FRIEND_SERVICE_HPP
#define FRIEND_SERVICE_HPP

#include <brpc/server.h>
#include <butil/logging.h>

#include "friend.pb.h"
#include "sql_chat_session.hpp"
#include "sql_chat_session_member.hpp"
#include "sql_friend_event.hpp"
#include "sql_friend_ralation.hpp"

#include "etcd.hpp"
#include "channel.hpp"
#include "utils.hpp"

namespace PROTOBUF_NAMESPACE_ID = google::protobuf;

class FriendServiceImp : public chat_im::FriendService
{

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

        //从用户服务模块请求获取好友列表的用户信息
        std::unordered_map<std::string,chat_im::UserInfo> user_info_list;
        bool isOk = __getMultiUserInfo(request_id,friend_list,user_info_list); 
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

                        // 设置request_id
                        response->set_request_id(request_id);

                        //数据库插入好友事件申请记录
                        std::string uuid = chat_im::util::uuid();
                        bool isOk = __friend_event_manager->insert(chat_im::friendEvent(uuid,user_id,respondent_id));
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
                        const std::string applu_user_id = request->apply_user_id();     //申请人 id
                        const bool agree = request->agree();

                        //判断是否有好友申请事件
                        std::shared_ptr<chat_im::friendEvent> event 
                                = __friend_event_manager->select_userIdAndFriendId(user_id,friend_id);
                        //没有找到申请事件
                        if(!event){
                                ERROR("没有查询到{}-{}的好友申请记录",user_id,friend_id);
                                return err_response("没有查询到此好友申请记录");
                        }
                        bool isOK;
                        //先处理 是否同意 同意创建会话信息等
                        if(agree){
                                //此insert中是插入俩次记录,即 user_id->friend_id和friend_id->user_id
                                isOk = __friend_ralation_manager->insert(chat_im::friendRelation(user_id,apply_ser_id));
                                if(!isOk){
                                     ERROR()
                                     return err_response();           
                                }
                                std::string chat_session_id = chat_im::util::uuid();
                                //插入新的会话信息 -- 好友会话
                                isOk = __chat_session_manager->insert(chat_im::chatSession(chat_session_id,"",chat_im::chatSessionType::SINGLE))
                                if(!isOk){
                                     ERROR()
                                     return err_response();           
                                }
                                
                                //插入俩条新的会话成员信息 -- user_id and friend_id
                                isOk =__chat_session_member_manager->insert(chat_im::chatSessionMember(chat_session_id,std::vector<std::string>({user_id,friend_id})));
                                if(!isOk){
                                     ERROR()
                                     return err_response();           
                                }
                                
                        }
                        //不同意

                        //需求文档要求是会删除请求记录,但我建议不删除
                        //TODO

                        }
    void FriendSearch(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                      const ::chat_im::FriendSearchReq *request,
                      ::chat_im::FriendSearchRsp *response,
                      ::google::protobuf::Closure *done);
    void GetChatSessionList(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                            const ::chat_im::GetChatSessionListReq *request,
                            ::chat_im::GetChatSessionListRsp *response,
                            ::google::protobuf::Closure *done);
    void ChatSessionCreate(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                           const ::chat_im::ChatSessionCreateReq *request,
                           ::chat_im::ChatSessionCreateRsp *response,
                           ::google::protobuf::Closure *done);
    void GetChatSessionMember(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                              const ::chat_im::GetChatSessionMemberReq *request,
                              ::chat_im::GetChatSessionMemberRsp *response,
                              ::google::protobuf::Closure *done);
    void GetPendingFriendEventList(::PROTOBUF_NAMESPACE_ID::RpcController *controller,
                                   const ::chat_im::GetPendingFriendEventListReq *request,
                                   ::chat_im::GetPendingFriendEventListRsp *response,
                                   ::google::protobuf::Closure *done);
private:
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

        chat_im::util::ServiceChannelManager::ptr __channel_manager;

        std::string __user_service_name;



};

class FriendService
{
};

class FriendServiceBuilder
{
};

#endif 

