#include <iostream>
#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "channel.hpp"  
#include "etcd.hpp"
#include "channel.hpp"
#include "logger.hpp"
#include "utils.hpp"


#include "base.pb.h"
#include "file.pb.h"
#include "user.pb.h"

using namespace chat_im;

DEFINE_bool(run_mode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(log_file, "", "发布模式下，用于指定日志的输出文件");
DEFINE_int32(log_level, 0, "发布模式下，用于指定日志输出等级");

DEFINE_string(etcd_host, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(base_service, "/service", "服务监控根目录");
DEFINE_string(user_service, "/service/user_service", "服务监控根目录");

chat_im::util::ServiceChannelManager::ptr chm;
std::string login_ssid;

std::string verify_code_id = "e534-dbd5adf0-0000";

// //测试手机号验证码获取
// TEST(test_phoneregister,get_verifycode){

//     PhoneVerifyCodeReq req;
//     req.set_request_id("100001");
//     req.set_phone_number("19120019387");

//     PhoneVerifyCodeRsp rsp;

//     auto channel = chm->choose(FLAGS_user_service);
//     //判断channel是否获取到
//     ASSERT_TRUE(channel);
//     UserService_Stub stub(channel.get());
//     brpc::Controller cntr;
//     stub.GetPhoneVerifyCode(&cntr,&req,&rsp,nullptr);



//     ASSERT_TRUE(rsp.success());
//     if(rsp.success()){
//         verify_code_id = rsp.verify_code_id();
//         DEBUG("验证码id:{}", rsp.verify_code_id());
//     }
//     ASSERT_EQ(rsp.request_id(),req.request_id());

// }

//
// //测试手机号注册
// // 先使用第一个get_verifycode进行获取验证码，然后修改验证码和验证码id进行测试
// TEST(test_phoneregister,phone_register){
//     //验证码id为空说明前面测试有问题
//     if(verify_code_id.empty()){
//         ASSERT_TRUE(false);
//         return ;
//     }
//     PhoneRegisterReq req;
//     req.set_request_id("100002");
//     req.set_phone_number("19120019387");
//     req.set_password("hlb030509");
//     std::string code;
//      std::cin>>code;
//     req.set_verify_code(code);
//     req.set_verify_code_id(verify_code_id);

//     auto channel = chm->choose(FLAGS_user_service);
//     //判断channel是否获取到
//     ASSERT_TRUE(channel);
//     UserService_Stub stub(channel.get());

//     PhoneRegisterRsp  rsp;

//     brpc::Controller cntl;
//     stub.PhoneRegister(&cntl,&req,&rsp,nullptr);

//     ASSERT_TRUE(rsp.success());
//     if(rsp.success()){
//         DEBUG("用户id{}",rsp.user_id());
//     }
//     ASSERT_EQ(rsp.request_id(),req.request_id());
// }

// TEST(user_login,user_id_password_login){
//     UserLoginReq req;
//     req.set_request_id("100003");
//     req.set_user_name("19120019387");
//     req.set_password("hlb030509");

//     auto channel = chm->choose(FLAGS_user_service);
//     // 判断channel是否获取到
//     ASSERT_TRUE(channel);
//     UserService_Stub stub(channel.get());

//     UserLoginRsp rsp;
//     brpc::Controller cntl;
//     stub.UserLogin(&cntl,&req,&rsp,nullptr);

//     ASSERT_TRUE(rsp.success());
//     if(rsp.success()){
//         login_ssid = rsp.login_session_id();
//         DEBUG("用户id{}",rsp.login_session_id());
//     }else{
//         ERROR("错误原因{}",rsp.errmsg());
//     }
//     ASSERT_EQ(rsp.request_id(),req.request_id());
// }

// //获取用户信息
// TEST(user_info,get_single_userInfo){
//     GetUserInfoReq req;
//     req.set_request_id("100004");
//     req.set_user_id("19120019387");

//     auto channel = chm->choose(FLAGS_user_service);
//     // 判断channel是否获取到
//     ASSERT_TRUE(channel);

//     UserService_Stub stub(channel.get());

//     brpc::Controller cntl;
//     GetUserInfoRsp rsp;

    
//     stub.GetUserInfo(&cntl,&req,&rsp,nullptr);

//     ASSERT_TRUE(rsp.success());
//     if(rsp.success()){
//         DEBUG("用户昵称{}，手机号{}",rsp.user_info().nickname(),rsp.user_info().phone());
//     }else{
//         ERROR("错误原因{}",rsp.errmsg());
//     }
//     ASSERT_EQ(rsp.request_id(),req.request_id());
// }

// TEST(user_info,get_multi_userInfo){
//     GetMultiUserInfoReq req;
//     req.set_request_id("100005");
//     req.add_users_id("19120019387");
//     req.add_users_id("haoyouxiaoju");

//     auto channel = chm->choose(FLAGS_user_service);
//     // 判断channel是否获取到
//     ASSERT_TRUE(channel);
//     UserService_Stub stub(channel.get());
//     brpc::Controller cntl;

//     GetMultiUserInfoRsp rsp;
//     stub.GetMultiUserInfo(&cntl,&req,&rsp,nullptr);


//     ASSERT_TRUE(rsp.success());
//     if(rsp.success()){
//         UserInfo info1 = (*rsp.mutable_users_info())["19120019387"];
//         UserInfo info2 = (*rsp.mutable_users_info())["haoyouxiaoju"];
//         DEBUG("用户1昵称{}，手机号{}",info1.nickname(),info1.phone());
//         DEBUG("用户2昵称{}，手机号{}",info2.nickname(),info2.phone());
//     }else{
//         ERROR("错误原因{}",rsp.errmsg());
//     }
//     ASSERT_EQ(rsp.request_id(),req.request_id());

// }

// TEST(set_userInfo,set_avatar){
//     SetUserAvatarReq req;
//     req.set_request_id("100006");
//     req.set_user_id("19120019387");
//     std::string avatar_body;
//     util::readFile("/home/xiaoju/program/tupian/haoyouxiaju.png",avatar_body);
//     if(avatar_body.compare("")==0){
//         ASSERT_TRUE(false);
//         return;
//     }
//     req.set_avatar(avatar_body);

//     auto channel = chm->choose(FLAGS_user_service);
//     // 判断channel是否获取到
//     ASSERT_TRUE(channel);
//     UserService_Stub stub(channel.get());
//     brpc::Controller cntl;

//     SetUserAvatarRsp rsp;
//     stub.SetUserAvatar(&cntl,&req,&rsp,nullptr);


//     ASSERT_TRUE(rsp.success());
//     if(rsp.success()){
//     }else{
//         ERROR("错误原因{}",rsp.errmsg());
//     }
//     ASSERT_EQ(rsp.request_id(),req.request_id());
// }

// TEST(set_userInfo,set_userNickname){
//     SetUserNicknameReq req;
//     req.set_request_id("100007");
//     req.set_user_id("19120019387");
//     req.set_nickname("jdkxm");

//     auto channel = chm->choose(FLAGS_user_service);
//     // 判断channel是否获取到
//     ASSERT_TRUE(channel);
//     UserService_Stub stub(channel.get());
//     brpc::Controller cntl;

//     SetUserNicknameRsp rsp;
//     stub.SetUserNickname(&cntl,&req,&rsp,nullptr);

//     ASSERT_TRUE(rsp.success());
//     if(rsp.success()){
//     }else{
//         ERROR("错误原因{}",rsp.errmsg());
//     }
//     ASSERT_EQ(rsp.request_id(),req.request_id());


// }

// TEST(set_userInfo,set_user_description){
//     SetUserDescriptionReq req;
//     req.set_request_id("100007");
//     req.set_user_id("19120019387");
//     req.set_description("jdkxm");

//     auto channel = chm->choose(FLAGS_user_service);
//     // 判断channel是否获取到
//     ASSERT_TRUE(channel);
//     UserService_Stub stub(channel.get());
//     brpc::Controller cntl;

//     SetUserDescriptionRsp rsp;
//     stub.SetUserDescription(&cntl,&req,&rsp,nullptr);

//     ASSERT_TRUE(rsp.success());
//     if(rsp.success()){
//     }else{
//         ERROR("错误原因{}",rsp.errmsg());
//     }
//     ASSERT_EQ(rsp.request_id(),req.request_id());

// }

// TEST(set_userInfo,set_user_phone){
//     SetUserPhoneNumberReq req;
//     req.set_request_id("100007");
//     req.set_user_id("19120019387");
//     req.set_phone_number("19120019387");
//     std::string code;
//     std::cin >> code;
//     req.set_phone_verify_code_id(verify_code_id);
//     req.set_phone_verify_code(code);

//     auto channel = chm->choose(FLAGS_user_service);
//     // 判断channel是否获取到
//     ASSERT_TRUE(channel);
//     UserService_Stub stub(channel.get());
//     brpc::Controller cntl;

//     SetUserPhoneNumberRsp rsp;
//     stub.SetUserPhoneNumber(&cntl,&req,&rsp,nullptr);

//     ASSERT_TRUE(rsp.success());
//     if(rsp.success()){
//     }else{
//         ERROR("错误原因{}",rsp.errmsg());
//     }
//     ASSERT_EQ(rsp.request_id(),req.request_id());


// }


int main(int argc,char* argv[]){

    google::ParseCommandLineFlags(&argc,&argv,true);
    chat_im::util::__init_logger__(FLAGS_run_mode,FLAGS_log_file,FLAGS_log_level);

    chm = std::make_shared<chat_im::util::ServiceChannelManager>();
    chm->declared(FLAGS_user_service);
    auto put_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOnline,chm.get(),std::placeholders::_1,std::placeholders::_2);
    auto del_cb = std::bind(&chat_im::util::ServiceChannelManager::onServiceOffline,chm.get(),std::placeholders::_1,std::placeholders::_2);

    chat_im::util::Discovery::ptr disc = std::make_shared<chat_im::util::Discovery>(FLAGS_etcd_host,FLAGS_base_service,put_cb,del_cb);

    
  
    testing::InitGoogleTest(&argc, argv);
    DEBUG("开始测试");


    return RUN_ALL_TESTS();
    return 0;

}