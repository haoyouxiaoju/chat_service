#include <iostream>

#include "user.pb.h"
#include "httplib.h"


int main(int argc,char* argv[]){

    

    chat_im::PhoneVerifyCodeReq req;
    req.set_request_id("100001");
    req.set_phone_number("19120019387");
    httplib::Client client("http://127.0.0.1:9000");
    httplib::Result res = client.Post("/service/user/get_phone_verify_code",req.SerializeAsString(),"application/x-protobuf");

    std::cout<<res.error();

    return 0;

}