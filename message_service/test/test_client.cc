#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "message_service.hpp"


DEFINE_bool(run_mode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(log_file, "", "发布模式下，用于指定日志的输出文件");
DEFINE_int32(log_level, 0, "发布模式下，用于指定日志输出等级");

DEFINE_string(registry_host, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(instance_name, "/message_service/instance", "当前实例名称");
DEFINE_string(access_host, "127.0.0.1:10005", "当前实例的外部访问地址");

DEFINE_string(base_service, "/service", "服务监控根目录");
DEFINE_string(file_service, "/service/file_service", "文件管理子服务名称");
DEFINE_string(user_service, "/service/user_service", "用户管理子服务名称");
DEFINE_string(message_service, "/service/message_service", "消息存储子服务名称");

chat_im::util::ServiceChannel::ChannelPtr message_channel;
auto start_time = boost::posix_time::from_iso_string("20250303T131256");
auto over_time = boost::posix_time::from_iso_string("20250306T131256");

TEST(get_msg,getHistoryMsg){
   chat_im::MsgStorageService_Stub stub(message_channel.get());
   chat_im::GetHistoryMsgReq req;
   req.set_request_id("100001");
   req.set_chat_session_id("");
   req.set_start_time(boost::posix_time::to_time_t(start_time));
   req.set_over_time(boost::posix_time::to_time_t(over_time));

   brpc::Controller cntl;
   chat_im::GetHistoryMsgRsp rsp;
   stub.GetHistoryMsg(&cntl,&req,&rsp,nullptr);

   if(!rsp.success()){
    ERROR("获取历史消息失败:{}",rsp.errmsg());
   }
   ASSERT_TRUE(rsp.success());
   DEBUG("获取{}条信息",rsp.message_list_size());
}


TEST(get_msg,getRecentMsg){
    chat_im::MsgStorageService_Stub stub(message_channel.get());
    chat_im::GetRecentMsgReq req;
    req.set_request_id("100002");
    req.set_chat_session_id("");
    req.set_msg_count(10);
 
    brpc::Controller cntl;
    chat_im::GetRecentMsgRsp rsp;
    stub.GetRecentMsg(&cntl,&req,&rsp,nullptr);
 
    if(!rsp.success()){
     ERROR("获取历史消息失败:{}",rsp.errmsg());
    }
    ASSERT_TRUE(rsp.success());
    DEBUG("获取{}条信息",rsp.message_list_size());
}
TEST(get_msg,MsgSearch){

    chat_im::MsgStorageService_Stub stub(message_channel.get());
    chat_im::MsgSearchReq req;
    req.set_request_id("100002");
    req.set_chat_session_id("");
    req.set_search_key("xiaoxi");
 
    brpc::Controller cntl;
    chat_im::MsgSearchRsp rsp;
    stub.MsgSearch(&cntl,&req,&rsp,nullptr);
 
    if(!rsp.success()){
     ERROR("获取历史消息失败:{}",rsp.errmsg());
    }
    ASSERT_TRUE(rsp.success());
    DEBUG("获取{}条信息",rsp.message_list_size());
}

int main(int argc,char* argv[]){

    google::ParseCommandLineFlags(&argc,&argv,true);
    chat_im::util::__init_logger__(FLAGS_run_mode,FLAGS_log_file,FLAGS_log_level);

    chat_im::util::ServiceChannelManager::ptr channel_manager   \
        = std::make_shared<chat_im::util::ServiceChannelManager>();
    //channel_manager->declared(FLAGS_file_service);
    //channel_manager->declared(FLAGS_user_service);
    channel_manager->declared(FLAGS_message_service);

    std::function<void(const std::string&,const std::string&)>    \
    put_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOnline,channel_manager.get(),std::placeholders::_1,std::placeholders::_2);
  std::function<void(const std::string&,const std::string&)>    \
    del_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOffline,channel_manager.get(),std::placeholders::_1,std::placeholders::_2);
  

    chat_im::util::Discovery disc(FLAGS_registry_host,FLAGS_base_service,put_cb,del_cb);


    //3. 通过Rpc信道管理对象，获取提供Echo服务的信道
    message_channel = channel_manager->choose(FLAGS_message_service);
    if (!message_channel) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return -1;
    }



    testing::InitGoogleTest(&argc, argv);
    DEBUG("开始测试");
    return RUN_ALL_TESTS();
}
