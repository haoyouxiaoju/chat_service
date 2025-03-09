#include <gtest/gtest.h>

#include "transmit_service.hpp"

DEFINE_bool(run_mode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(log_file, "", "发布模式下，用于指定日志的输出文件");
DEFINE_int32(log_level, 0, "发布模式下，用于指定日志输出等级");

DEFINE_string(registry_host, "http://127.0.0.1:2379", "服务注册中心地址");

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(base_service, "/service", "服务监控根目录");
DEFINE_string(transmite_service, "/service/transmite_service", "服务监控根目录");

chat_im::util::ServiceChannel::ChannelPtr channel;

TEST(push_message,push_text){
    chat_im::MsgTransmitService_Stub stub(channel.get());   
    chat_im::NewMessageReq req;
    req.set_request_id("10001");
    req.set_user_id("haoyouxiaoju");
    req.set_chat_session_id("2803-c450013d-0000");

    chat_im::MessageContent* content = req.mutable_message();
    content->set_message_type(chat_im::MessageType::STRING);
    content->mutable_string_message()->set_content("haoyouxiaoju 不吃牛肉");

    brpc::Controller cntl;
    chat_im::GetTransmitTargetRsp rsp;
    stub.GetTransmitTarget(&cntl,&req,&rsp,nullptr);

    ASSERT_TRUE(rsp.success());
    ASSERT_EQ("haoyouxiaoju 不吃牛肉",rsp.message().data().string_message().content());
    for(auto i = rsp.target_id_list().begin();i!=rsp.target_id_list().end();++i){
        DEBUG("会话成员:{}",*i);
    }


}

// TEST(push_message,push_file){

// }

// TEST(push_message,push_image){

// }

// TEST(push_message,push_speech){

// }

int main(int argc,char* argv[]){

    gflags::ParseCommandLineFlags(&argc,&argv,true);
    chat_im::util::__init_logger__(FLAGS_run_mode, FLAGS_log_file, FLAGS_log_level);
    chat_im::util::ServiceChannelManager::ptr channel_manager = std::make_shared<chat_im::util::ServiceChannelManager>();
    channel_manager->declared(FLAGS_transmite_service);

    std::function<void(const std::string &, const std::string &)>
        put_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOnline, channel_manager.get(), std::placeholders::_1, std::placeholders::_2);
    std::function<void(const std::string &, const std::string &)>
        del_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOffline, channel_manager.get(), std::placeholders::_1, std::placeholders::_2);

    chat_im::util::Discovery disc(FLAGS_registry_host, FLAGS_base_service, put_cb, del_cb);

    // 3. 通过Rpc信道管理对象，获取提供Echo服务的信道
    channel = channel_manager->choose(FLAGS_transmite_service);
    if (!channel)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return -1;
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}