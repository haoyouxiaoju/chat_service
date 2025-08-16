#!/bin/bash

#传递两个参数：
# 1. 可执行程序的路径名
# 2. 目录名称 --- 将这个程序的依赖库拷贝到指定目录下
declare depends
get_depends() {
    depends=$(ldd $1 | awk '{if (match($3,"/")){print $3}}')
    #mkdir $2
    cp -Lr $depends $2
}

get_depends ./gateway/build/gateway_service ./gateway/depends
get_depends ./file_service/build/files_service ./file_service/depends
get_depends ./friend_service/build/friend_service ./friend_service/depends
get_depends ./message_service/build/message_service ./message_service/depends
get_depends ./speech_service/build/speech_service ./speech_service/depends
get_depends ./messageTransmit_service/build/transmit_service ./messageTransmit_service/depends
get_depends ./user_service/build/user_service ./user_service/depends

cp /bin/nc ./gateway/
cp /bin/nc ./file_service/
cp /bin/nc ./friend_service/
cp /bin/nc ./message_service/
cp /bin/nc ./speech_service/
cp /bin/nc ./messageTransmit_service/
cp /bin/nc ./user_service/
get_depends /bin/nc ./gateway/depends
get_depends /bin/nc ./file_service/depends
get_depends /bin/nc ./friend_service/depends
get_depends /bin/nc ./message_service/depends
get_depends /bin/nc ./speech_service/depends
get_depends /bin/nc ./user_service/depends
get_depends /bin/nc ./messageTransmit_service/depends


