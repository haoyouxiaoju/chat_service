#ifndef DMS_ALI_HPP
#define DMS_ALI_HPP

#include <cstdlib>
#include <iostream>
#include <alibabacloud/core/AlibabaCloud.h>
#include <alibabacloud/core/CommonRequest.h>
#include <alibabacloud/core/CommonClient.h>
#include <alibabacloud/core/CommonResponse.h>
#include "logger.hpp"

namespace chat_im::util
{
    class DMSClient
    {
    public:
        using ptr = std::shared_ptr<DMSClient>;
        DMSClient(const std::string &key_id, const std::string &key_secret)
        {
            AlibabaCloud::InitializeSdk();
            AlibabaCloud::ClientConfiguration configuration("cn-shenzhen");
            // specify timeout when create client.
            configuration.setConnectTimeout(1500);
            configuration.setReadTimeout(4000);
            // Please ensure that the environment variables ALIBABA_CLOUD_ACCESS_KEY_ID and ALIBABA_CLOUD_ACCESS_KEY_SECRET are set.
            AlibabaCloud::Credentials credential(key_id, key_secret);
            /* use STS Token
            credential.setSessionToken( getenv("ALIBABA_CLOUD_SECURITY_TOKEN") );
            */
            __client = std::make_unique<AlibabaCloud::CommonClient>(credential, configuration);
        }
        ~DMSClient()
        {
            AlibabaCloud::ShutdownSdk();
        }
        bool send(const std::string &phone, const std::string &code)
        {
            AlibabaCloud::CommonRequest request(AlibabaCloud::CommonRequest::RequestPattern::RpcPattern);
            request.setHttpMethod(AlibabaCloud::HttpRequest::Method::Post);
            request.setDomain("dysmsapi.aliyuncs.com");
            request.setVersion("2017-05-25");
            request.setQueryParameter("Action", "SendSms");
            request.setQueryParameter("SignName", "好友小菊");
            request.setQueryParameter("TemplateCode", "SMS_478945683");
            request.setQueryParameter("PhoneNumbers", phone);
            request.setQueryParameter("TemplateParam", "{\"code\":\""+code+"\"}");

            auto response = __client->commonResponse(request);

            if(!response.isSuccess()){
                ERROR("短信发送-{}-失败原因:{}",phone,response.error().errorMessage().c_str());
                return false;
            }

            DEBUG("{}-发送验证码{}成功",phone,code);
            return true;

        }

    private:
        std::unique_ptr<AlibabaCloud::CommonClient> __client;
    };

} // namespace chat_im::util

#endif