cmake_minimum_required(VERSION 3.10)
link_directories(/usr/local/lib)


set(project_name test_lab)
project(${project_name})

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# proto 文件
set(proto_path ${CMAKE_CURRENT_SOURCE_DIR}/../proto/)
set(proto_files "")
list(APPEND proto_files
    user.proto
    base.proto
    )

# 检测pb文件是否生成
# set(pb_cc "")
# set(pb_h "")
set(pb_src "")
foreach(proto_file ${proto_files})
    string(REPLACE ".proto" ".pb.cc" pb_cc ${proto_file} )
    string(REPLACE ".proto" ".pb.h" pb_h ${proto_file} )
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${pb_cc})
        add_custom_command(
            PRE_BUILD
            COMMAND protoc   
            ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${proto_file} -I ${proto_path} --experimental_allow_proto3_optional
            DEPENDS ${proto_path}/${proto_file}
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${pb_cc}
            COMMENT "生成pb文件:"${pb_cc}
        )
    endif()
    list(APPEND pb_src ${CMAKE_CURRENT_BINARY_DIR}/${pb_cc})

endforeach()



#
# 源文件
set(project_src "")
# list(APPEND project_src speech_service.cc)  
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ project_src)
message(${project_src})

add_executable(${project_name} ${project_src} ${pb_src} ${odb_src})

# 设置需要连接的库
target_link_libraries(${project_name}  
	-lprotobuf -lleveldb 
	/usr/lib/x86_64-linux-gnu/libjsoncpp.so.19
	
	)


# # test
# set(test_project test_client)
# project(${test_project})
# set(test_files "")
# list(APPEND test_files ${CMAKE_CURRENT_SOURCE_DIR}/test/test_client.cc)
# add_executable(${test_project} ${test_files} ${pb_src} ${odb_src})
# target_link_libraries(${test_project} -lgtest -lgflags -lspdlog -lfmt -lbrpc -lssl  
#                  -lcrypto -lprotobuf -lleveldb -letcd-cpp-api -lcpprest -lcurl  
#                  -lodb-mysql -lodb -lodb-boost      
#                  -lalibabacloud-sdk-core -lcpr -lelasticlient
#                  -lhiredis -lredis++
#                   /usr/lib/x86_64-linux-gnu/libjsoncpp.so.19
#                   -lboost_system
#                   -lamqpcpp -lpthread -ldl -lev
#                   )


#头文件搜索路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../install/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../util/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../odb/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

