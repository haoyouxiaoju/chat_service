#ifndef FILES_SERVICE_HPP
#define FILES_SERVICE_HPP

#include <brpc/server.h>
#include <butil/logging.h>

#include "logger.hpp"
#include "etcd.hpp"
#include "utils.hpp"
#include "file.pb.h"




class FileServiceImpl : public chat_im::FileService{

public:

    FileServiceImpl(const std::string& base_path)
        :__base_filepath(base_path){
            umask(0);
            mkdir(base_path.c_str(), 0775);
            DEBUG("文件根目录{}",base_path);
            if (__base_filepath.back() != '/') __base_filepath.push_back('/');
        }
    ~FileServiceImpl(){}

    //
    //获取单个文件
    void GetSingleFile(::google::protobuf::RpcController *controller,
                       const ::chat_im::GetSingleFileReq *request,
                       ::chat_im::GetSingleFileRsp *response,
                       ::google::protobuf::Closure *done){
        brpc::ClosureGuard guard(done);

        std::string file_body;
        bool status = chat_im::util::readFile(__base_filepath+request->file_id(),file_body);
        response->set_request_id(request->request_id());
        if(status == false){
            ERROR("{}请求获取单文件{}失败",request->request_id(),request->file_id());
            response->set_errmsg("读取文件失败");
            response->set_success(false);
            return;
        }

        //success
        //
        response->set_success(true);
        response->mutable_file_data()->set_file_id(request->file_id());
        response->mutable_file_data()->set_file_content(file_body);


        return ;
    }
    
    //
    //获取多个文件
    void GetMultiFile(::google::protobuf::RpcController *controller,
                      const ::chat_im::GetMultiFileReq *request,
                      ::chat_im::GetMultiFileRsp *response,
                      ::google::protobuf::Closure *done){
        brpc::ClosureGuard guard(done);

        //
        int file_size = request->file_id_list().size();
        //
        //获取列表中每一个文件id来读取文件
        response->set_request_id(request->request_id());
        for(int i=0;i<file_size;++i){
            std::string file_id = request->file_id_list(i);
            std::string file_name = __base_filepath+file_id;
            
            std::string file_body;
            bool status = chat_im::util::readFile(file_name, file_body);
            if (status == false){
                ERROR("{} 读取文件数据失败！", request->request_id());
                response->set_success(false);
                response->set_errmsg("读取文件数据失败！");
                return;
            }

            chat_im::FileDownLoadData info;
            info.set_file_id(file_id);
            info.set_file_content(file_body);
            response->mutable_file_data()->insert(::google::protobuf::MapPair<std::string,chat_im::FileDownLoadData>({file_id,info}));

        }
        //success
        //
        response->set_success(true);
        return ;


    }
    
    //
    //上传单个文件
    void PutSingleFile(::google::protobuf::RpcController *controller,
                       const ::chat_im::PutSingleFileReq *request,
                       ::chat_im::PutSingleFileRsp *response,
                       ::google::protobuf::Closure *done){
        brpc::ClosureGuard guard(done);

        response->set_request_id(request->request_id());

        std::string file_id = chat_im::util::uuid();
        std::string file_name = __base_filepath+file_id;
        bool status = chat_im::util::writeFile(file_name,request->file_data().file_content());
        if(status == false){
            ERROR("上传文件请求{}:写入失败",request->request_id());
            response->set_success(false);
            response->set_errmsg("写入文件数据失败！");
            return;
        }

        response->set_success(true);
        response->mutable_file_info()->set_file_id(file_id);
        response->mutable_file_info()->set_file_size(request->file_data().file_size());
        response->mutable_file_info()->set_file_name(request->file_data().file_name());
        return ;

    }
    
    //
    //上传多个文件
    void PutMultiFile(::google::protobuf::RpcController *controller,
                      const ::chat_im::PutMultiFileReq *request,
                      ::chat_im::PutMultiFileRsp *response,
                      ::google::protobuf::Closure *done){
        brpc::ClosureGuard guard(done);

        response->set_request_id(request->request_id());
        int _files_size = request->file_data().size();
        for(int i=0;i<_files_size;++i){
            std::string file_id = chat_im::util::uuid();
            std::string file_name = __base_filepath + file_id;
            bool status = chat_im::util::writeFile(file_name,request->file_data(i).file_content());
            if(status == false ){
                ERROR("上传多个文件请求{}:写入失败", request->request_id());
                response->set_success(false);
                response->set_errmsg("写入文件数据失败！");
                return ;
            }
            chat_im::FileMessageInfo* info = response->add_file_info();
            info->set_file_id(file_id);
            info->set_file_size(request->file_data(i).file_size());
            info->set_file_name(request->file_data(i).file_name());

        }
        response->set_success(true);
        return ;

    }

private:
    std::string __base_filepath;

};


class FileService{
public:
    using ptr = std::shared_ptr<FileService>;
    FileService(const chat_im::util::Registry::ptr &reg_client,const std::shared_ptr<brpc::Server>& rpc_service)
        :__reg_client(reg_client),__rpc_service(rpc_service)
    {}
    ~FileService(){}

    void start(){
        __rpc_service->RunUntilAskedToQuit();
    }


private:
    chat_im::util::Registry::ptr __reg_client;
    std::shared_ptr<brpc::Server> __rpc_service;
};


class FileServiceBuilder{
public:
    void make_regClient(const std::string &reg_host,    \
                        const std::string& service_name,     \
                        const std::string& access_host){
        __reg_client =std::make_shared<chat_im::util::Registry>(reg_host);
        __reg_client->registry(service_name,access_host);
    }
    void make_rpcService(uint16_t port,int32_t timeout,uint8_t num_threads,const std::string& base_path){

        __rpc_service = std::make_shared<brpc::Server>();

        FileServiceImpl* impl = new FileServiceImpl(base_path);
        bool ret =__rpc_service->AddService(impl,brpc::ServiceOwnership::SERVER_OWNS_SERVICE);
        if(ret == -1){
            ERROR("rpc服务添加失败");
            abort();
        }


        brpc::ServerOptions opt;
        opt.idle_timeout_sec = timeout;
        opt.num_threads = num_threads;
        ret = __rpc_service->Start(port,&opt);
        if(ret == -1){
            ERROR("服务启动失败");
            abort();
        }
    }
    FileService::ptr build()
    {
        
        if(!__reg_client){
            ERROR("未初始化服务注册模块");
            abort();
        }
        if(!__rpc_service){
            ERROR("未初始化rpc服务器模块");
            abort();
        }
        return std::make_shared<FileService>(__reg_client,__rpc_service);
    }

private:
    chat_im::util::Registry::ptr __reg_client;
    std::shared_ptr<brpc::Server> __rpc_service;

};






#endif