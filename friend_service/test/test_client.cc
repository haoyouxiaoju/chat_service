#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <thread>
#include "etcd.hpp"
#include "channel.hpp"
#include "utils.hpp"
#include "friend.pb.h"

DEFINE_bool(run_mode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(log_file, "", "发布模式下，用于指定日志的输出文件");
DEFINE_int32(log_level, 0, "发布模式下，用于指定日志输出等级");

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(base_service, "/service", "服务监控根目录");
DEFINE_string(friend_service, "/service/friend_service", "服务监控根目录");

chat_im::util::ServiceChannelManager::ptr channel_manager; 
std::string notify_event_id1,notify_event_id2,new_session_id,cssid;

// //添加好友
// TEST(friend_op,add_friendreq1){
//     auto channel = channel_manager->choose(FLAGS_friend_service);
//     ASSERT_TRUE(channel);
//     if (!channel) {
//         ERROR("获取通信信道失败！");
//         return;
//     }

//     chat_im::FriendAddReq req;
//     req.set_request_id("10001");
//     req.set_user_id("haoyouxiaoju");
//     req.set_respondent_id("19120019387");

//     chat_im::FriendAddRsp rsp;

//     brpc::Controller cntl;
//     chat_im::FriendService_Stub stub(channel.get());
//     stub.FriendAdd(&cntl,&req,&rsp,nullptr);

//     ASSERT_TRUE(rsp.success());
//     if(!rsp.success()){
//         ERROR("失败:{}",rsp.errmsg());
//     }

//    notify_event_id1 = rsp.notify_event_id();
//    DEBUG("{}",notify_event_id1);

// }

// //添加好友2
// TEST(friend_op,add_friendreq2){
//     auto channel = channel_manager->choose(FLAGS_friend_service);
//     ASSERT_TRUE(channel);
//     if (!channel) {
//         ERROR("获取通信信道失败！");
//         return;
//     }

//     chat_im::FriendAddReq req;
//     req.set_request_id("10005");
//     req.set_user_id("haoyouxiaoju");
//     req.set_respondent_id("17602024902");

//     chat_im::FriendAddRsp rsp;

//     brpc::Controller cntl;
//     chat_im::FriendService_Stub stub(channel.get());
//     stub.FriendAdd(&cntl,&req,&rsp,nullptr);

//     ASSERT_TRUE(rsp.success());
//     if(!rsp.success()){
//         ERROR("失败:{}",rsp.errmsg());
//     }

//    notify_event_id2 = rsp.notify_event_id();
//    DEBUG("{}",notify_event_id2);

// }

// //获取待处理的好友申请列表
// TEST(friend_op,get_pending_friend_event){
//     auto channel = channel_manager->choose(FLAGS_friend_service);
//     ASSERT_TRUE(channel);
//     if (!channel) {
//         ERROR("获取通信信道失败！");
//         return;
//     }
//     brpc::Controller cntl;
//     chat_im::FriendService_Stub stub(channel.get());

//     chat_im::GetPendingFriendEventListReq req;
//     req.set_request_id("10002");
//     req.set_user_id("19120019387");
//     chat_im::GetPendingFriendEventListRsp rsp;
//     stub.GetPendingFriendEventList(&cntl,&req,&rsp,nullptr);

//     ASSERT_TRUE(rsp.success());
//     if(!rsp.success()){
//         ERROR("失败:{}",rsp.errmsg());
//         return;
//     }
//     for(auto i = rsp.event().begin();i!=rsp.event().end();++i){
//         DEBUG("event_id{}-sender_id{}",i->event_id(),i->sender().user_id());
//     }

// }

// //处理好友申请  -- 同意
// TEST(friend_op,agree_friend_vent){
//     auto channel = channel_manager->choose(FLAGS_friend_service);
//     ASSERT_TRUE(channel);
//     if (!channel) {
//         ERROR("获取通信信道失败！");
//         return;
//     }
//     brpc::Controller cntl;
//     chat_im::FriendService_Stub stub(channel.get());

//     chat_im::FriendAddProcessReq req;
//     req.set_request_id("10003");
//     req.set_notify_event_id(notify_event_id1);
//     req.set_agree(true);
//     req.set_apply_user_id("haoyouxiaoju");
//     req.set_user_id("19120019387");

//     chat_im::FriendAddProcessRsp rsp;
//     stub.FriendAddProcess(&cntl,&req,&rsp,nullptr);

//     ASSERT_TRUE(rsp.success());
//     if(!rsp.success()){
//         ERROR("失败:{}",rsp.errmsg());
//         return;
//     }
//     new_session_id = rsp.new_session_id();
//     DEBUG("会话id{}",rsp.new_session_id());
// }
// //处理好友申请  -- 拒绝
// TEST(friend_op,unagree_friend_vent){
//     auto channel = channel_manager->choose(FLAGS_friend_service);
//     ASSERT_TRUE(channel);
//     if (!channel) {
//         ERROR("获取通信信道失败！");
//         return;
//     }
//     brpc::Controller cntl;
//     chat_im::FriendService_Stub stub(channel.get());

//     chat_im::FriendAddProcessReq req;
//     req.set_request_id("10004");
//     req.set_notify_event_id(notify_event_id2);
//     req.set_agree(false);
//     req.set_apply_user_id("haoyouxiaoju");
//     req.set_user_id("17602024902");

//     chat_im::FriendAddProcessRsp rsp;
//     stub.FriendAddProcess(&cntl,&req,&rsp,nullptr);


//     ASSERT_TRUE(rsp.success());
//     if(!rsp.success()){
//         ERROR("失败:{}",rsp.errmsg());
//         return;
//     }
// }

// //获取好友列表
// TEST(friend_op,get_friend_list){
//     auto channel = channel_manager->choose(FLAGS_friend_service);
//     ASSERT_TRUE(channel);
//     if (!channel) {
//         ERROR("获取通信信道失败！");
//         return;
//     }
//     brpc::Controller cntl;
//     chat_im::FriendService_Stub stub(channel.get());

//     chat_im::GetFriendListReq req;
//     req.set_request_id("10006");
//     req.set_user_id("haoyouxiaoju");

//     chat_im::GetFriendListRsp rsp;
//     stub.GetFriendList(&cntl,&req,&rsp,nullptr);  

//     ASSERT_TRUE(rsp.success());
//     if(!rsp.success()){
//         ERROR("失败:{}",rsp.errmsg());
//         return;
//     }

//     ASSERT_EQ("19120019387",rsp.friend_list(0).user_id());
//     for(auto i = rsp.friend_list().begin();i!=rsp.friend_list().end();++i){
//         DEBUG("好友id{}",i->user_id());
//     }
// }

//搜索用户
TEST(friend_op,search_friend){
    auto channel = channel_manager->choose(FLAGS_friend_service);
    ASSERT_TRUE(channel);
    if (!channel) {
        ERROR("获取通信信道失败！");
        return;
    }
    brpc::Controller cntl;
    chat_im::FriendService_Stub stub(channel.get());

    chat_im::FriendSearchReq req;
    req.set_request_id("10006");
    req.set_user_id("haoyouxiaoju");
    req.set_search_key("17602024902");

    chat_im::FriendSearchRsp rsp;
    stub.FriendSearch(&cntl,&req,&rsp,nullptr);  

    ASSERT_TRUE(rsp.success());
    if(!rsp.success()){
        ERROR("失败:{}",rsp.errmsg());
        return;
    }
    
    // ASSERT_EQ("17602024902",rsp.user_info(0).user_id());
    for(auto i = rsp.user_info().begin();i!=rsp.user_info().end();++i){
        DEBUG("用户id{}",i->user_id());
    }

}

// //获取会话列表
// TEST(chat_session_op,get_chat_sessionList){
//     auto channel = channel_manager->choose(FLAGS_friend_service);
//     ASSERT_TRUE(channel);
//     if (!channel) {
//         ERROR("获取通信信道失败！");
//         return;
//     }
//     brpc::Controller cntl;
//     chat_im::FriendService_Stub stub(channel.get());

//     chat_im::GetChatSessionListReq req;
//     req.set_request_id("10007");
//     req.set_user_id("haoyouxiaoju");

//     chat_im::GetChatSessionListRsp rsp;
//     stub.GetChatSessionList(&cntl,&req,&rsp,nullptr);  

//     ASSERT_TRUE(rsp.success());
//     if(!rsp.success()){
//         ERROR("失败:{}",rsp.errmsg());
//         return;
//     }
    
//     ASSERT_EQ(new_session_id,rsp.chat_session_info_list(0).chat_session_id());
//     for(auto i = rsp.chat_session_info_list().begin();i!=rsp.chat_session_info_list().end();++i){
//         DEBUG("会话id{}",i->chat_session_id());
//     }
// }

// //创建会话
// TEST(chat_session_op,create_chat_session){

//     auto channel = channel_manager->choose(FLAGS_friend_service);
//     ASSERT_TRUE(channel);
//     if (!channel) {
//         ERROR("获取通信信道失败！");
//         return;
//     }
//     brpc::Controller cntl;
//     chat_im::FriendService_Stub stub(channel.get());

//     chat_im::ChatSessionCreateReq req;
//     req.set_request_id("10008");
//     req.set_user_id("haoyouxiaoju");
//     req.set_chat_session_name("不吃牛肉");
//     req.add_member_id_list("19120019387");
//     req.add_member_id_list("17602024902");

//     chat_im::ChatSessionCreateRsp rsp;
//     stub.ChatSessionCreate(&cntl,&req,&rsp,nullptr);  

//     ASSERT_TRUE(rsp.success());
//     if(!rsp.success()){
//         ERROR("失败:{}",rsp.errmsg());
//         return;
//     }
//     cssid = rsp.chat_session_info().chat_session_id();
//     DEBUG("会话id{}", rsp.chat_session_info().chat_session_id());
// }

// //获取会话成员
// TEST(chat_session_op,get_chat_member){

//     auto channel = channel_manager->choose(FLAGS_friend_service);
//     ASSERT_TRUE(channel);
//     if (!channel) {
//         ERROR("获取通信信道失败！");
//         return;
//     }
//     brpc::Controller cntl;
//     chat_im::FriendService_Stub stub(channel.get());

//     chat_im::GetChatSessionMemberReq req;
//     req.set_request_id("10009");
//     req.set_user_id("haoyouxiaoju");
//     req.set_chat_session_id(cssid);
    

//     chat_im::GetChatSessionMemberRsp rsp;
//     stub.GetChatSessionMember(&cntl,&req,&rsp,nullptr);  

//     ASSERT_TRUE(rsp.success());
//     if(!rsp.success()){
//         ERROR("失败:{}",rsp.errmsg());
//         return;
//     }

//     for(auto i = rsp.member_info_list().begin();i!=rsp.member_info_list().end();++i){
//         DEBUG("会话id{}",i->user_id());
//     }
   
// }

// //删除好友
// TEST(chat_session_op,remove_friend){

//     auto channel = channel_manager->choose(FLAGS_friend_service);
//     ASSERT_TRUE(channel);
//     if (!channel) {
//         ERROR("获取通信信道失败！");
//         return;
//     }
//     brpc::Controller cntl;
//     chat_im::FriendService_Stub stub(channel.get());

//     chat_im::FriendRemoveReq req;
//     req.set_request_id("10010");
//     req.set_user_id("haoyouxiaoju");
//     req.set_peer_id("19120019387");
    

//     chat_im::FriendRemoveRsp rsp;
//     stub.FriendRemove(&cntl,&req,&rsp,nullptr);  

//     ASSERT_TRUE(rsp.success());
//     if(!rsp.success()){
//         ERROR("失败:{}",rsp.errmsg());
//         return;
//     }
   
// }

int main(int argc,char* argv[]){

    google::ParseCommandLineFlags(&argc, &argv, true);
    chat_im::util::__init_logger__(FLAGS_run_mode, FLAGS_log_file, FLAGS_log_level);

    
    //1. 先构造Rpc信道管理对象
    channel_manager = std::make_shared<chat_im::util::ServiceChannelManager>();
    channel_manager->declared(FLAGS_friend_service);
    auto put_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOnline, channel_manager.get(), std::placeholders::_1, std::placeholders::_2);
    auto del_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOffline, channel_manager.get(), std::placeholders::_1, std::placeholders::_2);
    //2. 构造服务发现对象
    chat_im::util::Discovery::ptr dclient = std::make_shared<chat_im::util::Discovery>(FLAGS_etcd_host, FLAGS_base_service, put_cb, del_cb);
    
    

    testing::InitGoogleTest(&argc,argv);
    
    return RUN_ALL_TESTS();
}