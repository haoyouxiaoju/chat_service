#ifndef USER_HXX
#define USER_HXX

#include <odb/nullable.hxx>
#include <odb/core.hxx>

#include <string>

namespace chat_im{


#pragma db object
class User{

public:
    //注册--只可手机号注册
    User(const std::string& phone,const std::string& password,  \
        const std::string& user_id,const std::string& nickname)
    :__phone(phone),__password(password),   \
     __user_id(user_id),__nickname(nickname) {}
    User(){}

    std::string user_id(){return __user_id;}
    void user_id(const std::string& value){ __user_id = value;}

    std::string phone(){return __phone;}
    void phone(const std::string& value){__phone = value;}

    std::string password(){return __password;}
    void password(const std::string& value){__password = value;}

    std::string nickname(){return __nickname;}
    void nickname(const std::string& value){__nickname = value;}

    std::string signature(){
        if(!__signature) 
            return std::string();
        return *__signature;
    }
    void signature(const std::string& value){__signature = value;}

    std::string avatar_id(){return __avatar_id;}
    void avatar_id(const std::string& value){__avatar_id = value;}

private:

private:
    friend class odb::access;

    #pragma db id auto
    unsigned long __id;

    #pragma db unique index unique type("varchar(128)")  //comment("用户id 即用户名可用于登录")
    std::string __user_id;//用户id 即用户名可用于登录,可修改且唯一

    #pragma db unique index unique type("char(11)") //comment("绑定的手机号码")
    std::string __phone;        //只采取手机号注册
    //使用手机号注册后,同时需要设置密码
    //
    //后续可添加换绑手机号功能

    #pragma db type("varchar(128)") //comment("用户密码")
    std::string __password;     
    //无论是手机号还是用户名都可采取密码登录
    //手机号额外验证码登录

    #pragma db type("varchar(128)")  //comment("用户昵称,采用初始时随机生成")
    std::string __nickname;     //不可用于登录

    #pragma db type("varchar(128)") //comment("用户签名")
    odb::nullable<std::string> __signature;//用户签名

    #pragma db type("varchar(128)") //comment("用户头像id,用于搜索用户头像文件")
    std::string __avatar_id;
    // 初始默认的黑色头像
    // 后续用户自定义修改





};
//odb -d mysql --generate-query --generate-schema --profile boost/date-time \
//     user.hxx
}

#endif