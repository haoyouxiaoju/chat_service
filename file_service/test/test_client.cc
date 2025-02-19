#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <thread>

#include "etcd.hpp"
#include "channel.hpp"
#include "logger.hpp"
#include "utils.hpp"

#include "file.pb.h"
#include "base.pb.h"

DEFINE_bool(run_mode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(log_file, "", "发布模式下，用于指定日志的输出文件");
DEFINE_int32(log_level, 0, "发布模式下，用于指定日志输出等级");

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(base_service, "/service", "服务监控根目录");
DEFINE_string(file_service, "/service/file_service", "服务监控根目录");


util::ServiceChannel::ChannelPtr channel;
std::string single_file_id;
std::vector<std::string> multi_file_id;

//
//  测试上传一个文件
TEST(put_test,single_file){

    //读取文件内容
    std::string body;
    ASSERT_TRUE(util::readFile("./Makefile",body));
    chat_im::FileService_Stub stub(channel.get());

    chat_im::PutSingleFileReq req;
    req.set_request_id("000001");
    req.mutable_file_data()->set_file_name("Makefile");
    req.mutable_file_data()->set_file_size(body.size());
    req.mutable_file_data()->set_file_content(body);

    brpc::Controller *cntl = new brpc::Controller();
    chat_im::PutSingleFileRsp* rsp =new chat_im::PutSingleFileRsp();
    stub.PutSingleFile(cntl,&req,rsp,nullptr);
    ASSERT_FALSE(cntl->Failed());

    //
    //assert
    ASSERT_TRUE(rsp->success());
    ASSERT_EQ(rsp->file_info().file_size(), body.size());
    ASSERT_EQ(rsp->file_info().file_name(), "Makefile");
    single_file_id = rsp->file_info().file_id();
    DEBUG("文件ID：{}", rsp->file_info().file_id());

}

//
//测试多个文件上传
TEST(put_test,multi_file){

    //读取文件内容
    std::string body1,body2;
    ASSERT_TRUE(util::readFile("./base.pb.h",body1));
    ASSERT_TRUE(util::readFile("./file.pb.h",body2));


    //
    chat_im::FileService_Stub stub(channel.get());

    chat_im::PutMultiFileReq req;
    req.set_request_id("0000002");

    chat_im::FileUpLoadData* data1 = req.add_file_data();
    data1->set_file_name("base.pb.h");
    data1->set_file_size(body1.size());
    data1->set_file_content(body1);

    chat_im::FileUpLoadData* data2 = req.add_file_data();
    data2->set_file_name("file.pb.h");
    data2->set_file_size(body2.size());
    data2->set_file_content(body2);

    //调用
    brpc::Controller *cntl = new brpc::Controller();
    chat_im::PutMultiFileRsp* rsp =new chat_im::PutMultiFileRsp();
    stub.PutMultiFile(cntl,&req,rsp,nullptr);

   //
    //assert
    ASSERT_TRUE(rsp->success());
    for(int i=0;i<rsp->file_info_size();++i){
        multi_file_id.push_back(rsp->file_info(i).file_id());
        DEBUG("文件id:{}",multi_file_id[i]);
    }

}

TEST(get_test,single_file){
    chat_im::FileService_Stub stub(channel.get());

    chat_im::GetSingleFileReq req;
    req.set_request_id("0000003");
    req.set_file_id(single_file_id);
    brpc::Controller* cntl = new brpc::Controller();
    chat_im::GetSingleFileRsp* rsp = new chat_im::GetSingleFileRsp();
    stub.GetSingleFile(cntl,&req,rsp,nullptr);
    ASSERT_TRUE(rsp->success());
    ASSERT_EQ(single_file_id,rsp->file_data().file_id());
    util::writeFile("makefile_get",rsp->file_data().file_content());

}

TEST(get_test,multi_file){
    chat_im::FileService_Stub stub(channel.get());

    chat_im::GetMultiFileReq req;
    req.set_request_id("00000004");
    req.add_file_id_list(multi_file_id[0]);
    req.add_file_id_list(multi_file_id[1]);

    brpc::Controller* cntl = new brpc::Controller();
    chat_im::GetMultiFileRsp* rsp = new chat_im::GetMultiFileRsp();
    stub.GetMultiFile(cntl,&req,rsp,nullptr);

    ASSERT_TRUE(rsp->success());
    ASSERT_TRUE(rsp->file_data().find(multi_file_id[0]) != rsp->file_data().end());
    ASSERT_TRUE(rsp->file_data().find(multi_file_id[1]) != rsp->file_data().end());

    auto map = rsp->file_data();
    util::writeFile("filepbh",map[multi_file_id[0]].file_content());
    util::writeFile("filepbh",map[multi_file_id[1]].file_content());


}


int main(int argc,char* argv[]){

    testing::InitGoogleTest(&argc,argv);

    google::ParseCommandLineFlags(&argc,&argv,true);
    util::__init_logger__(FLAGS_run_mode, FLAGS_log_file, FLAGS_log_level);

    // 1. 先构造Rpc信道管理对象
    std::shared_ptr<util::ServiceChannelManager> manager = std::make_shared<util::ServiceChannelManager>();
    manager->declared(FLAGS_file_service);
    // 2. 构造服务发现对象
    auto put_cb =  std::bind(&util::ServiceChannelManager::onServiceOnline,manager.get(),std::placeholders::_1,std::placeholders::_2);
    auto del_cb =  std::bind(&util::ServiceChannelManager::onServiceOffline,manager.get(),std::placeholders::_1,std::placeholders::_2);

    util::Discovery::ptr dclient = std::make_shared<util::Discovery>(FLAGS_etcd_host, FLAGS_base_service, put_cb, del_cb);
    //3. 通过Rpc信道管理对象，获取提供Echo服务的信道
    channel = manager->choose(FLAGS_file_service);
    if (!channel) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return -1;
    }


    return RUN_ALL_TESTS();
}
