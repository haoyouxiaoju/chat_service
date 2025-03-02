#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <atomic>
#include <random>
#include <iomanip>

#include "logger.hpp"

namespace chat_im::util{

//返回一个uuid
std::string uuid() {
    //生成一个由16位随机字符组成的字符串作为唯一ID
    // 1. 生成6个0~255之间的随机数字(1字节-转换为16进制字符)--生成12位16进制字符
    std::random_device rd;//实例化设备随机数对象-用于生成设备随机数
    std::mt19937 generator(rd());//以设备随机数为种子，实例化伪随机数对象
    std::uniform_int_distribution<int> distribution(0,255); //限定数据范围

    std::stringstream ss;
    for (int i = 0; i < 6; i++) {
        if (i == 2) ss << "-";
        ss << std::setw(2) << std::setfill('0') << std::hex << distribution(generator);
    }
    ss << "-";
    // 2. 通过一个静态变量生成一个2字节的编号数字--生成4位16进制数字字符
    static std::atomic<short> idx(0);
    short tmp = idx.fetch_add(1);
    ss << std::setw(4) << std::setfill('0') << std::hex << tmp;
    return ss.str();
}

std::string verify_code(){
    std::random_device rd;//实例化设备随机数对象-用于生成设备随机数
    std::mt19937 generator(rd());//以设备随机数为种子，实例化伪随机数对象
    std::uniform_int_distribution<int> distribution(0,9); //限定数据范围

    std::stringstream ss;
    for (int i = 0; i < 4; i++) {
        ss << distribution(generator);
    }
    return ss.str();
}


//
//读取文件  
bool readFile(const std::string& file_path,std::string& body){
    DEBUG("进入readFile开始读取");
    DEBUG("读取的文件名{}",file_path);
    std::ifstream file(file_path,std::ios_base::binary | std::ios_base::in);
    if(!file.is_open()){
        ERROR("读取文件{}:文件打开失败.",file_path);
        return false;
    }
    //获取文件大小
    file.seekg(0,std::ios_base::end);
    std::streampos len = file.tellg();
    file.seekg(0,std::ios_base::beg);
    //提前设置body长度以防止多次扩容
    body.resize(len);
    file.read(&body[0],len);
    if(file.good() == false){
        ERROR("{}文件读取失败",file_path);
        file.close();
        return false;
    }

    file.close();
    return true;
}

//
//写入文件
bool writeFile(const std::string& file_path,const std::string& body){
    std::ofstream file(file_path,std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
    if(!file.is_open()){
        ERROR("写入文件{}:文件打开失败",file_path);
        return false;
    }

    file.write(body.c_str(),body.size());

    if(file.good() == false){
        ERROR("写入{}文件失败",file_path);
        file.close();
        return false;
    }

    file.close();
    return true;


}

}

#endif