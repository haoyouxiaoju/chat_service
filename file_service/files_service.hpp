#ifndef FILES_SERVICE_HPP
#define FILES_SERVICE_HPP

#include <brpc/server.h>
#include <butil/logging.h>
#include <openssl/sha.h>
#include <bthread/mutex.h>
#include <bthread/condition_variable.h>

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
        // 防止因file_id为空导致程序崩溃需要加一判断
        if (request->file_id().compare("") == 0)
        {
            ERROR("{}请求文件所设置的file_id为空");
            response->set_success(false);
            response->set_errmsg("所设置的file_id为空！");
            return;
            }
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
            //防止因file_id为空导致程序崩溃需要加一判断
            std::string file_id = request->file_id_list(i);
            if(file_id.compare("")==0){
                ERROR("{}请求文件所设置的file_id为空");
                response->set_success(false);
                response->set_errmsg("所设置的file_id为空！");
                return;
            }
            std::string file_name = __base_filepath+file_id;
            
            std::string file_body;
            DEBUG("开始读取文件{}",file_name);
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
        DEBUG("{}:{}",request->file_data().file_name(),request->file_data().file_size());
        response->set_request_id(request->request_id());
        response->mutable_file_info()->set_file_size(request->file_data().file_size());
        response->mutable_file_info()->set_file_name(request->file_data().file_name());
        
        StoreResult r = StoreFile(request->file_data().file_content());
        DEBUG("store result status:{},file_id:{}",r.success,r.file_id);
        response->set_success(r.success); 
        //存储失败
        if(!r.success){
            DEBUG("存储失败:{}",r.errmsg);
            response->set_errmsg(r.errmsg);
            return;
        }
        response->mutable_file_info()->set_file_id(r.file_id);
        DEBUG("store file is successed");
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
            DEBUG("{}:{}", request->file_data(i).file_name(), request->file_data(i).file_size());
            StoreResult r = StoreFile(request->file_data(i).file_content());
            if(!r.success){
                response->set_success(false);
                response->set_errmsg(r.errmsg);
                return ;
            }
            chat_im::FileMessageInfo* info = response->add_file_info();
            info->set_file_id(r.file_id);
            info->set_file_size(request->file_data(i).file_size());
            info->set_file_name(request->file_data(i).file_name());

        }
        response->set_success(true);
        return ;

    }

private:
    /**
     * 用于文件存储状态
     */
    struct FileState
    {
        bool finished = false;
        bool success = false;
        std::string errmsg;
        bthread::ConditionVariable cv;
        bthread::Mutex mtx;
    };
    /**
     * 存储返回
     */
    struct StoreResult
    {
        bool success;
        std::string file_id;
        std::string errmsg;
    };
    // 计算文件内容的 SHA256 哈希
    std::string calculateFileHash(const std::string& content) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256((const unsigned char*)content.data(), content.size(), hash);
        return std::string(reinterpret_cast<char*>(hash), SHA256_DIGEST_LENGTH);
    }
    
    /**
     * 存储文件，包含哈希去重、并发控制、写入逻辑
     */
    StoreResult StoreFile(const std::string& file_content) {
        StoreResult result;
    
        std::string hash_file = calculateFileHash(file_content);
        DEBUG("存储文件的哈希值:{}",hash_file);
        std::string file_id = chat_im::util::uuid();
        std::shared_ptr<FileState> file_state;
        bool need_wait = false;
        {
            // std::lock_guard lock(__file_mutex);
            std::lock_guard<bthread::Mutex> lock(__file_mutex);
            auto ite = __file_hashMap.find(hash_file);
            /**
             * 可能已经存储过,检查是否是其他插入key值,但还没存储完毕
             */
            if(ite != __file_hashMap.end()){
                file_id = ite->second;
                //判断是否提取插入的值
                auto s_ite = __writing_files.find(file_id);
                if(s_ite == __writing_files.end()){
                    /**
                     * 直接返回
                     */
                    result.success = true;
                    result.file_id = file_id;
                    return result;
                }else{
                    /**
                     * 需要等待
                     */
                    file_state = s_ite->second;
                    need_wait = true;
                }
            }else{
                /**
                 * 没有存储过,提前插入map中
                 * 避免多个线程同时进行写入同一文件
                 */
                __file_hashMap[hash_file] = file_id;
                file_state = std::make_shared<FileState>();
                __writing_files[file_id] = file_state;
            }
        }
        /**
         * 等到另一线程写入完毕
         */
        DEBUG("finished {},use count {}",file_state->finished,file_state.use_count());
        if (need_wait){
            DEBUG("wait file {}",file_id);
            std::unique_lock<bthread::Mutex> lock(file_state->mtx);
            butil::Timer timer;
            timer.start();
            constexpr int wait_time_ms = 3000;
            while(!file_state->finished){
                int64_t remaining_ms = wait_time_ms - timer.m_elapsed()/1000;
                if(remaining_ms <= 0){
                    result.success = false;
                    result.errmsg = "文件处理超时";
                    return result;
                }
                int wait_result = file_state->cv.wait_for(lock,remaining_ms);
                if(wait_result == ETIMEDOUT){
                    continue;
                }else if(wait_result != 0){
                    result.success = false;
                    result.errmsg = "等待被中断";
                    return result;
                }
            }
            result.success = file_state->success;
            result.errmsg = file_state->errmsg;
            result.file_id = file_id;
            return result;
        }
        /**
         * 存储文件,
         */
        std::string file_name = __base_filepath+file_id;
        DEBUG("存储的文件名:{}",file_name);
        bool status = chat_im::util::writeFile(file_name,file_content);
        {
            // std::lock_guard lock(file_state->mtx);
            std::lock_guard<bthread::Mutex> lock(file_state->mtx);
            file_state->finished = true;
            file_state->success = status;
            if(!status){
                file_state->errmsg = "写入文件失败";
            }
        }
        /**
         * 通知等待的线程
         */
        file_state->cv.notify_all();
        
        {
            std::lock_guard<bthread::Mutex> lock(__file_mutex);
            __writing_files.erase(file_id);
            if(status == false){
                /**
                 * 取消原先插入的map值
                 */
                __file_hashMap.erase(hash_file);
                result.success = false;
                result.errmsg = "写入文件数据失败！";
                return result;
            }
        }
    
        DEBUG("存储的文件成功:{}",file_name);
        result.success = true;
        result.file_id = file_id;
        return result;
    }


private:
    std::string __base_filepath;
    std::unordered_map<std::string,std::string> __file_hashMap;
    std::unordered_map<std::string, std::shared_ptr<FileState>> __writing_files;
    bthread::Mutex __file_mutex;

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
