#! /bin/bash


# # 定义要执行的命令数组
# commands=(
#     "./gateway/build/gateway_service -flagfile=./conf/gateway_server.conf"
#     "./file_service/build/files_service -flagfile=./conf/file_server.conf"
#     "./friend_service/build/friend_service -flagfile=./conf/friend_server.conf"
#     "./message_service/build/message_service -flagfile=./conf/message_server.conf"
#     "./speech_service/build/speech_service -flagfile=./conf/speech_server.conf"
#     "./messageTransmit_service/build/transmit_service -flagfile=./conf/transmite_server.conf"
#     "./user_service/build/user_service -flagfile=./conf/user_server.conf"
# )

# 定义要执行的命令数组
commands=(
    "./gateway/build/gateway_service "
    "./file_service/build/files_service "
    "./friend_service/build/friend_service"
    "./message_service/build/message_service "
    "./speech_service/build/speech_service "
    "./messageTransmit_service/build/transmit_service "
    "./user_service/build/user_service "
)
# 遍历命令数组并在后台执行每个命令
for command in "${commands[@]}"; do
    echo "正在启动命令: $command"
    # 添加 & 使命令在后台运行
    $command &
    # 检查是否成功启动
    if [ $? -ne 0 ]; then
        echo "错误：命令 $command 启动失败！"
        exit 1
    fi
done

echo "所有命令已在后台启动。"
# 使用 wait 等待所有后台进程完成（可选）
#wait
