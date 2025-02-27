#ifndef USER_SERVICE_HPP
#define USER_SERVICE_HPP


#include "etcd.hpp"
#include "channel.hpp"
#include "logger.hpp"
#include "data_redis.hpp"
#include "user.hxx"
#include "user-odb.hxx"
#include "mysql/sql_user.hpp"
#include "data_es.hpp"
#include "utils.hpp"
#include "dms.hpp"

#include "base.pb.h"
#include "user.pb.h"
#include "file.pb.h"

#include <regex>
#include <functional>

const std::string PHONE_NUMBER_RULE = "\d{11}";
const std::string NICKNAME_RULE = "";
const std::string VERIFY_CODE_RULE = "";
const std::string DEFALUT_AVATAR_ID ="";

class UserServiceImpl:public chat_im::UserService {
public:
    UserServiceImpl(){}
    ~UserServiceImpl(){}

    
  //  密码登录,用户名可以是手机号也可以是用户名
  void UserLogin(::google::protobuf::RpcController* controller,
               const ::chat_im::UserLoginReq* request,
               ::chat_im::UserLoginRsp* response,
               ::google::protobuf::Closure* done){
                brpc::ClosureGuard guard(done);

                std::function<void(const std::string&)>   \
                err_response = [response](const std::string& err_msg)->void{
                  response->set_errmsg(err_msg);
                  response->set_success(false);
                  return;
                };

                const std::string request_id = request->request_id();
                DEBUG("收到{}请求进行密码登录",request_id);
                response->set_request_id(request_id);

                //
                const std::string user_name = request->user_name();
                const std::string password = request->password();

                // 先进行手机号查询,如果没有则进行用户名查询
                std::shared_ptr<chat_im::User> user = __user_manager->select_by_phone(user_name);
                if(!user){
                  user = __user_manager->select_by_userId(user_name);
                  //用户名错误
                  if(!user){
                    //
                    ERROR("{}用户名登陆请求:{}-用户名错误",request_id,user_name);
                    return err_response("用户名错误");
                  }
                }
                //判断密码正确
                //错误
                if(user->password().compare(password) != 0){
                  //
                    ERROR("{}用户名登陆请求:{}-密码错误",request_id,user_name);
                    return err_response("密码错误");
                }

                //redis中查询该用户的状态
                //用于request中的user_name是用户id或手机号,所以不采用user_name来设置;
                bool isOk = __status_manager->isExist(user->user_id());
                if(isOk){
                  //
                  //
                    ERROR("{}用户名登陆请求:{}-用户已登录",request_id,user_name);
                    //不考虑顶号
                    return err_response("用户已被登录");
                }

                //生成会话id
                const std::string sessiono_id = chat_im::util::uuid();
                __session_manager->append(sessiono_id,user->user_id());
                __status_manager->append(user->user_id());

                //完成,并返回
                response->set_success(true);
                response->set_login_session_id(sessiono_id);

               }
  
  void GetPhoneVerifyCode(::google::protobuf::RpcController* controller,
               const ::chat_im::PhoneVerifyCodeReq* request,
               ::chat_im::PhoneVerifyCodeRsp* response,
               ::google::protobuf::Closure* done){

                brpc::ClosureGuard guard(done);

                const std::string phone = request->phone_number();
                const std::string request_id = request->request_id();

                //设置请求id
                response->set_request_id(request_id);

                std::function<void(const std::string&)> 
                  err_response([response](const std::string& err_msg)->void{
                    response->set_success(false);
                    response->set_errmsg(err_msg);
                    return;
                  });

                  //判断手机号码合法性
                bool isOk = std::regex_match(phone,std::regex(PHONE_NUMBER_RULE));

                if(!isOk){
                  //
                  ERROR("手机号码错误:{}",phone);
                  return err_response("手机号码错误");
                }

                //生成验证码并发送
                const std::string code = chat_im::util::verify_code();
                isOk = __dms_manager->send(phone,code);

                if(!isOk){
                  //
                  ERROR("{}验证码发送失败",phone);
                  return err_response("验证码发送失败");
                }

                //生成验证码id并加入redis
                const std::string code_id = chat_im::util::uuid();
                __code_manager->append(code_id,code);

                response->set_success(true);
                response->set_verify_code_id(code_id);

               }

  //  手机号码注册--          
  void PhoneRegister(::google::protobuf::RpcController* controller,
               const ::chat_im::PhoneRegisterReq* request,
               ::chat_im::PhoneRegisterRsp* response,
               ::google::protobuf::Closure* done){

                brpc::ClosureGuard guard(done);
                
                std::function<void(const std::string&)>   \
                err_response  = \
                [response](const std::string& err_msg)->void{
                  response->set_success(false);
                  response->set_errmsg(err_msg);
                  return ;
                };

                std::string request_id = request->request_id();
                DEBUG("收到{}请求进行手机号注册",request_id);
                response->set_request_id(request_id);
                //
                //判断 手机号码和验证码是否合法
                //
                std::string phone = request->phone_number();
                bool isok = std::regex_match(phone,std::regex(PHONE_NUMBER_RULE));
                //  手机号不合法
                if(!isok){
                  //
                  //
                  ERROR("请求{}:手机号码不合法{}",request_id,phone);
                  return err_response("手机号码不合法");

                }

                //TODO
                //考虑添加密码合法性判断


                std::string verify_code = request->verify_code();
                isok = std::regex_match(verify_code,std::regex(VERIFY_CODE_RULE));
                //  验证码不合法
                if(!isok){
                  //
                  //
                  ERROR("请求{}:验证码不合法{}",request_id,verify_code);
                  return err_response("验证码不合法");

                }

                //判断验证码与 reids内存储的验证码是否一致
                std::string verify_code_redis = __code_manager->code(request->verify_code_id()).value();
                //不一致
                if(verify_code_redis.compare(verify_code) != 0){
                  //
                  //
                  ERROR("请求{}:验证码错误{}",request_id,verify_code);
                  return err_response("验证码错误");

                }

                //判断手机号是否已经被注册 , 数据库内查询
                std::shared_ptr<chat_im::User> user(__user_manager->select_by_phone(phone));
                if(!user){
                  //
                  //
                  ERROR("请求{}:手机号码已被注册{}",request_id,phone);
                  return err_response("手机号码已被注册");

                }       
                
                //
                // 向数据库新增数据
                std::string nickname = std::string("用户").append(phone);
                std::string user_id = phone;
                std::string password = request->password();
                chat_im::User insert_data(phone,password,user_id,nickname);
                insert_data.avatar_id(DEFALUT_AVATAR_ID);

                isok = __user_manager->insert(insert_data);
                if(!isok){
                  //
                  //
                  ERROR("请求{}:数据库新增数据失败{}",phone);
                  return err_response("数据库新增数据失败");
                }

                //
                //向es服务器新增用户信息
                isok = __esUser_manager->append(nickname,user_id,phone,"",DEFALUT_AVATAR_ID);
                if(!isok){
                  //
                  //
                  ERROR("请求{}:ES服务器新增数据失败{}",phone);
                  return err_response("ES服务器新增数据失败");
                }

                //成功完成添加,返回响应
                response->set_success(true);
                response->set_user_id(user_id);

               }
  
  void PhoneLogin(::google::protobuf::RpcController* controller,
               const ::chat_im::PhoneLoginReq* request,
               ::chat_im::PhoneLoginRsp* response,
               ::google::protobuf::Closure* done){

                brpc::ClosureGuard guard(done);

                std::function<void(const std::string&)>   \
                err_response = [response](const std::string& err_msg)->void{
                  response->set_errmsg(err_msg);
                  response->set_success(false);
                  return;
                };

                const std::string request_id = request->request_id();
                const std::string phone = request->phone_number();
                const std::string code_id = request->verify_code_id();
                const std::string code = request->verify_code();

                response->set_request_id(request_id);
                bool isOk = std::regex_match(phone,std::regex(PHONE_NUMBER_RULE));

                if(!isOk){
                  //
                  ERROR("手机号码错误:{}",phone);
                  return err_response("手机号码错误");
                }

                const std::string code_redis = *(__code_manager->code(code_id));
                if(code_redis.compare(code) != 0){
                  ERROR("手机登录{}:验证码错误",phone);
                  return err_response("验证码错误");
                }

                std::shared_ptr<chat_im::User> user =  __user_manager->select_by_phone(phone);
                if(!user){
                  ERROR("手机号{}未注册",phone);
                  return err_response("手机号未注册");
                }

                const std::string session_id = chat_im::util::uuid();
                __session_manager->append(session_id,user->user_id());

                response->set_login_session_id(session_id);
                response->set_success(true);
  }

  // 获取单个用户的信息             
  void GetUserInfo(::google::protobuf::RpcController* controller,
               const ::chat_im::GetUserInfoReq* request,
               ::chat_im::GetUserInfoRsp* response,
               ::google::protobuf::Closure* done){
                brpc::ClosureGuard gurad(done);

                std::function<void(const std::string&)> \
                err_response =  \
                [response](const std::string& err_msg)->void{
                  response->set_success(false);
                  response->set_errmsg(err_msg);
                  return;
                };

                const std::string request_id = request->request_id();
                DEBUG("收到{}请求获取单个用户信息",request_id);
                //设置请求id
                response->set_request_id(request_id);

                //id
                const std::string user_id = request->user_id();
                //查询用户信息
                std::shared_ptr<chat_im::User>  \
                  user_data(__user_manager->select_by_userId(user_id));
                //未查询到用户的信息
                if(!user_data){
                  //
                  ERROR("{}请求用户信息失败:用户{}不存在",request_id,user_id);
                  return err_response("用户不存在");
                }

                //
                //构建用户信息
                chat_im::UserInfo* info(response->mutable_user_info());

                chat_im::GetSingleFileRsp rsp;
                bool isOk = this->__downloadFile(request_id,user_data->avatar_id(), rsp);
                if(!isOk){
                  ERROR("本地下载{}头像数据失败",user_id);
                  return err_response("获取用户数据失败");
                }
                // 获取头像数据失败
                if (!rsp.success())
                {
                  //
                  ERROR("{}请求用户信息失败:获取{}用户头像失败",request_id,user_id);
                  return err_response("获取用户数据失败");
                }
                info->set_avatar(rsp.file_data().file_content());

                info->set_user_id(user_data->user_id());
                info->set_nickname(user_data->nickname());
                info->set_description(user_data->signature());
                info->set_phone(user_data->phone());

                //完成request_id and user_info
                response->set_success(true);
                //end
               }
  
  // 获取多个用户的信息             
  void GetMultiUserInfo(::google::protobuf::RpcController* controller,
               const ::chat_im::GetMultiUserInfoReq* request,
               ::chat_im::GetMultiUserInfoRsp* response,
               ::google::protobuf::Closure* done)
               {
                brpc::ClosureGuard guard(done);
                std::function<void(const std::string&)> \
                err_response([response](const std::string& err_msg)->void{
                  response->set_success(false);
                  response->set_errmsg(err_msg);
                  return;
                });

                //设置请求id
                const std::string request_id(request->request_id());
                DEBUG("收到{}请求获取多个用户信息",request_id);
                response->set_request_id(request_id);

                
                //获取用户id
                size_t id_size = request->users_id().size();
                std::vector<std::string> userId_list(id_size);
                for(int i=0;i<id_size;++i){
                  userId_list.push_back(request->users_id(i));
                }

                //获取用户数据
               std::vector<chat_im::User>     \
                userData_list(__user_manager->select_multi_users(userId_list));
               //获取用户数据个数 与用户id个数不一致 则错误
                if(userData_list.size() != id_size ){
                //
                ERROR("{}请求:获取用户数据个数{}错误--用户id个数{}",request_id,userData_list.size(),id_size);
                return err_response("获取数据失败");
               } 

               //构建头像id数组
               std::vector<std::string> avatarId_list(id_size);
               for(int i=0;i<id_size;++i){
                avatarId_list.push_back(userData_list[i].avatar_id());
               }

               //获取头像的二进制数据
               chat_im::GetMultiFileRsp rsp;
               bool isOk = __downloadFiles(request_id,avatarId_list,rsp);
               if(!isOk){
                  ERROR("本地下载头像数据失败");
                  return err_response("获取数据失败");
               }
               if(!rsp.success()){
                ERROR("{}请求:获取多个头像数据失败",request_id);
                return err_response("获取数据失败");
               }

              google::protobuf::Map<std::string,chat_im::UserInfo>*   \
                users_info =response->mutable_users_info();
              google::protobuf::Map<std::string,chat_im::FileDownLoadData>*  \
               users_avatar = rsp.mutable_file_data();
               //构建返回的用户信息
               //TODO
               for(int i=0;i<id_size;++i){
                const std::string uid = userData_list[i].user_id();
                //
                //根据map []操作符特性获取或构建对应的userinfo的对象
                chat_im::UserInfo& user = (*users_info)[uid];
                user.set_user_id(uid);
                user.set_nickname(userData_list[i].nickname());
                user.set_description(userData_list[i].signature());
                user.set_phone(userData_list[i].phone());
                user.set_avatar((*users_avatar)[uid].file_content());

               }
               response->set_success(true);

               }
  
  // 修改用户的头像             
  void SetUserAvatar(::google::protobuf::RpcController* controller,
               const ::chat_im::SetUserAvatarReq* request,
               ::chat_im::SetUserAvatarRsp* response,
               ::google::protobuf::Closure* done){

                brpc::ClosureGuard guard(done);

                //设置请求id
                const std::string request_id = request->request_id();
                response->set_request_id(request_id);

                std::function<void(const std::string&)>   \
                  err_response([response](const std::string& err_msg)->void{
                    response->set_success(false);
                    response->set_errmsg(err_msg);
                    return;
                  });

                const std::string user_id = request->user_id();

                DEBUG("{}请求修改{}用户头像",request_id,user_id);

                //
                //获取用户id对应的用户信息
                std::shared_ptr<chat_im::User>     \
                  user(__user_manager->select_by_userId(user_id));
                //没查询到对应用户
                if(!user){
                  //
                  ERROR("{}未查询到对应的用户",user_id);
                  return err_response("修改头像失败:用户不存在");
                }

                chat_im::PutSingleFileRsp rsp;
                bool isOk = __uploadFiles(request_id,user_id,request->avatar(),rsp);
                if(!isOk){
                  ERROR("上传{}头像数据失败",user_id);
                  return err_response("获取数据失败");
                }

                //上传头像成功返回的文件id
                const std::string file_id(rsp.file_info().file_id());

                //
                //修改数据库
                user->avatar_id(file_id);
                isOk = __user_manager->update(user);
                if(!isOk){
                  ERROR("更新数据库内{}用户头像id失败",user_id);
                  return err_response("修改失败");
                }

                //修改ES服务器内用户信息
                isOk = __esUser_manager->append(user);
                if(!isOk){
                  ERROR("更新ES服务器内{}用户信息",user_id);
                  return err_response("修改失败");
                }

                response->set_success(true);

               }


  void SetUserNickname(::google::protobuf::RpcController* controller,
               const ::chat_im::SetUserNicknameReq* request,
               ::chat_im::SetUserNicknameRsp* response,
               ::google::protobuf::Closure* done){

                brpc::ClosureGuard guard(done);
                const std::string request_id = request->request_id();
                const std::string user_id = request->user_id();
                const std::string nickname = request->nickname();

                DEBUG("{}请求{}用户修改昵称{}",request_id,user_id,nickname);
                response->set_request_id(request_id);
                std::function<void(const std::string&)> \
                 err_response([response](const std::string& err_msg)->void{
                  response->set_success(false);
                  response->set_errmsg(err_msg);
                  return ;
                 });

                //检查昵称的合法性
                bool isOk = std::regex_match(nickname,std::regex(NICKNAME_RULE));
                if(!isOk){
                  //
                  //
                  ERROR("{}用户要修改成的昵称不合法:{}",user_id,nickname);
                  return err_response("昵称不合法");
                }

                //从数据库中查询对应的用户
                std::shared_ptr<chat_im::User>  \
                  user(__user_manager->select_by_userId(user_id));
                if(!user){
                  //
                  ERROR("{}未查询到对应的用户",user_id);
                  return err_response("修改头像失败:用户不存在");
                }

                //修改用户名称
                user->nickname(nickname);

                //修改数据库
                isOk = __user_manager->update(user);
                if(!isOk){
                  //
                  ERROR("更新数据库内{}用户昵称失败",user_id);
                  return err_response("修改失败");

                }

                //修改ES服务器
                isOk = __esUser_manager->append(user);
                if(!isOk){
                  //
                  ERROR("更新ES服务器内{}用户信息",user_id);
                  return err_response("修改失败");

                }

                response->set_success(true);


               }


  void SetUserDescription(::google::protobuf::RpcController* controller,
               const ::chat_im::SetUserDescriptionReq* request,
               ::chat_im::SetUserDescriptionRsp* response,
               ::google::protobuf::Closure* done){

                brpc::ClosureGuard guard(done);
                const std::string request_id = request->request_id();
                const std::string user_id = request->user_id();
                const std::string description = request->description();

                DEBUG("{}请求{}用户修改签名{}",request_id,user_id,description);
                response->set_request_id(request_id);
                std::function<void(const std::string&)> \
                 err_response([response](const std::string& err_msg)->void{
                  response->set_success(false);
                  response->set_errmsg(err_msg);
                  return ;
                 });

                //从数据库中查询对应的用户
                std::shared_ptr<chat_im::User>  \
                  user(__user_manager->select_by_userId(user_id));
                if(!user){
                  //
                  ERROR("{}未查询到对应的用户",user_id);
                  return err_response("修改头像失败:用户不存在");
                }

                //修改用户签名
                user->signature(description);

                //修改数据库
                bool isOk = __user_manager->update(user);
                if(!isOk){
                  //
                  ERROR("更新数据库内{}用户签名失败",user_id);
                  return err_response("修改失败");

                }

                //修改ES服务器
                isOk = __esUser_manager->append(user);
                if(!isOk){
                  //
                  ERROR("更新ES服务器内{}用户信息",user_id);
                  return err_response("修改失败");

                }

                response->set_success(true);

               }
  void SetUserPhoneNumber(::google::protobuf::RpcController* controller,
               const ::chat_im::SetUserPhoneNumberReq* request,
               ::chat_im::SetUserPhoneNumberRsp* response,
               ::google::protobuf::Closure* done){

                brpc::ClosureGuard guard(done);
                const std::string request_id = request->request_id();
                const std::string user_id = request->user_id();
                const std::string phone = request->phone_number();
                const std::string verify_code = request->phone_verify_code();
                const std::string verify_code_id = request->phone_verify_code_id();

                DEBUG("{}请求{}用户修改手机号{}",request_id,user_id,phone);
                response->set_request_id(request_id);
                std::function<void(const std::string&)> \
                 err_response([response](const std::string& err_msg)->void{
                  response->set_success(false);
                  response->set_errmsg(err_msg);
                  return ;
                 });

                 //获取reids中的验证码并判断
                std::string verify_code_redis = *(__code_manager->code(verify_code_id));

                if(verify_code_redis.compare(verify_code) != 0){
                  //
                  ERROR("{}修改手机号码失败:验证码错误",user_id);
                  return err_response("验证码错误");

                }


                //从数据库中查询对应的用户
                std::shared_ptr<chat_im::User>  \
                  user(__user_manager->select_by_userId(user_id));
                if(!user){
                  //
                  ERROR("{}未查询到对应的用户",user_id);
                  return err_response("修改头像失败:用户不存在");
                }

                //修改用户名称
                user->phone(phone);

                //修改数据库
                bool isOk = __user_manager->update(user);
                if(!isOk){
                  //
                  ERROR("更新数据库内{}用户签名失败",user_id);
                  return err_response("修改失败");

                }

                //修改ES服务器
                isOk = __esUser_manager->append(user);
                if(!isOk){
                  //
                  ERROR("更新ES服务器内{}用户信息",user_id);
                  return err_response("修改失败");

                }

                response->set_success(true);
               }

private:
    // 此处用于获取头像二进制数据
    bool __downloadFile(const std::string& request_id,\
                        const std::string& file_id,\
                        chat_im::GetSingleFileRsp& rsp){
      brpc::Channel* channel = __channel_manager->choose(__file_service_name).get(); 
      if(channel == nullptr){
        //
        ERROR("未获取到文件服务channel");
        return false;
      }        
      chat_im::FileService_Stub stub(channel);
      //构建请求
      chat_im::GetSingleFileReq req;
      req.set_request_id(request_id);
      req.set_file_id(file_id);

      brpc::Controller cntl;
      //远程调用
      stub.GetSingleFile(&cntl,&req,&rsp,nullptr);
      return true;
    }
    // 此处用于获取多个头像二进制数据
    bool __downloadFiles(const std::string& request_id,\ 
                         const std::vector<std::string>& files_id,\
                         chat_im::GetMultiFileRsp& rsp){
      
      brpc::Channel* channel = __channel_manager->choose(__file_service_name).get();
      if(channel == nullptr){
        //
        ERROR("未获取到文件服务channel");
        return false;
      }
      chat_im::FileService_Stub stub(channel);
      //构建请求
      chat_im::GetMultiFileReq req;
      req.set_request_id(request_id);
      for(int i=0;i<files_id.size();++i){
        req.set_file_id_list(i,files_id[i]);
      }

      brpc::Controller cntl;
      stub.GetMultiFile(&cntl,&req,&rsp,nullptr);
      return true;
    }

    // 此处用于上传头像文件
    bool __uploadFiles(const std::string& request_id, \
                       const std::string& user_id,  \
                       const std::string& file_content,   \
                       chat_im::PutSingleFileRsp& rsp){
      brpc::Channel* channel = __channel_manager->choose(__file_service_name).get();
      if(channel == nullptr){
        //
        ERROR("未获取到文件服务channel");
        return false;
      }              
      chat_im::FileService_Stub stub(channel);
      //构建文件上传请求
      chat_im::PutSingleFileReq req;
      req.set_request_id(request_id);
      req.set_user_id(user_id);
      chat_im::FileUpLoadData* data = req.mutable_file_data();
      data->set_file_name(user_id);
      data->set_file_size(file_content.size());
      data->set_file_content(file_content);
      
      brpc::Controller cntl;
      stub.PutSingleFile(&cntl,&req,&rsp,nullptr);
      return true;

    }


private:
    chat_im::util::Code::ptr __code_manager;
    chat_im::util::Status::ptr __status_manager;
    chat_im::util::Session::ptr __session_manager;
    chat_im::util::UserTable::ptr __user_manager;
    chat_im::util::ESUser::ptr __esUser_manager;
    std::string __file_service_name;
    chat_im::util::ServiceChannelManager::ptr __channel_manager;
    chat_im::util::DMSClient::ptr __dms_manager;

    

};

class UserService{
public:

private:


};

class IserServiceBuilder{
public:


private:
};


#endif