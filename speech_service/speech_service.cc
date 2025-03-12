#include "speech_service.hpp"
#include <gflags/gflags.h>

DEFINE_bool(logger_mode,false,"logger 模式");
DEFINE_string(logger_outFile,"","logger log输出文件地址");
DEFINE_int32(logger_level,0,"logger level");

//  etcd
DEFINE_string(etcd_host,"http://127.0.0.1:2379","服务注册地址");
DEFINE_string(base_service,"/service","服务监听根目录");
DEFINE_string(instance_name,"/speech_service/instance","当前实例名称");
DEFINE_string(instance_host,"127.0.0.1:10001","当前实例的外部访问地址");

//
//rpc
DEFINE_int32(lister_port,10001,"");
DEFINE_int32(rpc_timeout,-1,"");
DEFINE_int32(rpc_threads_num,1,"");

//
// aip语言
DEFINE_string(app_id,"6281148","");
DEFINE_string(aip_key,"S8HFyKKZrbQUJ4lxVn9bYEWR","");
DEFINE_string(secret_key,"SeOp6UsVKE63SlKxipdRHC6ZLROC7lqh","");


int main(int argc,char* argv[]){
    google::ParseCommandLineFlags(&argc,&argv,true);

    chat_im::util::__init_logger__(FLAGS_logger_mode,FLAGS_logger_outFile,FLAGS_logger_level);

    // logging::LoggingSettings stt;
    // stt.logging_dest=logging::LoggingDestination::LOG_TO_NONE;
    // logging::InitLogging(stt);
    SpeechServiceBuilder builder;
    builder.make_asrClient(FLAGS_app_id,FLAGS_aip_key,FLAGS_secret_key);
    builder.make_regClient(FLAGS_etcd_host,FLAGS_base_service+FLAGS_instance_name,FLAGS_instance_host);
    builder.make_rpcService(FLAGS_lister_port,FLAGS_rpc_timeout,FLAGS_rpc_threads_num);

    SpeechService::ptr server = builder.build();
    server->start();


    return 0;
}