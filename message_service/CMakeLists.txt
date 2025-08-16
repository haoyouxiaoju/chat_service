cmake_minimum_required(VERSION 3.10)
link_directories(/usr/local/lib)

# 在每个子模块的CMakeLists.txt开头添加：
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)

message("message service 的根文件"${CMAKE_CURRENT_SOURCE_DIR})


set(project_name message_service)
project(${project_name})

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# proto 文件
set(proto_path ${CMAKE_CURRENT_SOURCE_DIR}/../proto/)
set(proto_files "")
list(APPEND proto_files base.proto message.proto file.proto user.proto)

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

#odb 文件
set(odb_path ${CMAKE_CURRENT_SOURCE_DIR}/../odb/)
set(odb_files "")
list(APPEND odb_files message.hxx user.hxx)

message("odb文件:"${odb_path}${odb_files})
# 检测odb文件是否生成
set(odb_src "")
foreach(odb_file ${odb_files})
    string(REPLACE ".hxx" "-odb.cxx" odb_cxx ${odb_file})
    string(REPLACE ".hxx" "-odb.hxx" odb_hxx ${odb_file})
    string(REPLACE ".hxx" "-odb.ixx" odb_ixx ${odb_file})
    string(REPLACE ".hxx" ".sql" odb_sql ${odb_file})
    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${odb_hxx})
        add_custom_command(
            PRE_BUILD
            COMMAND odb
            ARGS -d mysql --generate-query --std c++11 --generate-schema --profile boost/date-time ${odb_path}/${odb_file}
            DEPENDS ${odb_path}/${odb_file}
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${odb_cxx} 
            COMMENT "生成"${odb_cxx}
        )
    endif()
    list(APPEND odb_src ${CMAKE_CURRENT_BINARY_DIR}/${odb_cxx})
endforeach()

message("odb文件:"${odb_src})


#
# 源文件
set(project_src "")
list(APPEND project_src message_service.cc)  
# aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ project_src)
message(${project_src})

add_executable(${project_name} ${project_src} ${pb_src} ${odb_src})

# 设置需要连接的库
target_link_libraries(${project_name} -lgflags 
	-lspdlog -lfmt -lbrpc -lssl -lcrypto 
	-lprotobuf -lleveldb -letcd-cpp-api 
	-lcpprest -lcurl -lodb-mysql -lodb -lodb-boost
	/usr/lib/x86_64-linux-gnu/libjsoncpp.so.19
	-lcpr -lelasticlient
	-lboost_system
	-lamqpcpp -lpthread -ldl -lev)


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

