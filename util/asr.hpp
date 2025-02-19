#ifndef ASR_HPP
#define ASR_HPP

#include "aip-cpp-sdk/speech.h"
#include "logger.hpp"


// 设置APPID/AK/SK
static const std::string aip_app_id = "6281148";
static const std::string aip_api_key = "S8HFyKKZrbQUJ4lxVn9bYEWR";
static const std::string aip_secret_key = "SeOp6UsVKE63SlKxipdRHC6ZLROC7lqh";

namespace util{
class asrClient{
public:
    using ptr = std::shared_ptr<asrClient>;
    asrClient(const std::string& app_id,    \
                const std::string& api_key, \
                const std::string& secret_key)
        :__client(app_id,api_key,secret_key){}
    ~asrClient(){}
    std::string recognize(std::string file_content,std::string& err_msg){
        Json::Value result = __client.recognize(file_content, "pcm", 16000, aip::null);
        if(result["err_no"].asInt() != 0){
            ERROR("语言识别出错:{}",result["err_msg"].asString());
            err_msg = result["err_msg"].asString();
            return nullptr;
        }
        return result["result"][0].asString();

    }
private:

    aip::Speech __client;

};

}



#endif