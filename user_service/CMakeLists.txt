cmake_minimum_required(VERSION 3.10)

set(project_name user_service)
project(${project_name})

# proto 文件
set(proto_path ${CMAKE_CURRENT_SOURCE_DIR}/../proto/)
set(proto_files "")
list(APPEND proto_files user.proto file.proto base.proto)

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
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${pb_cc} ${CMAKE_CURRENT_BINARY_DIR}/${pb_h}
            COMMENT "生成pb文件:"${pb_cc}
        )
    endif()
    list(APPEND pb_src ${CMAKE_CURRENT_BINARY_DIR}/${pb_cc})

endforeach()

#odb 文件
set(odb_path ${CMAKE_CURRENT_SOURCE_DIR}/../odb/)
set(odb_files "")
list(APPEND odb_files user.hxx)

# 检测odb文件是否生成
set(odb_src "")
foreach(odb_file ${odb_files})
    string(REPLACE ".hxx" "-odb.cxx" odb_cxx ${odb_files})
    string(REPLACE ".hxx" "-odb.hxx" odb_hxx ${odb_files})
    string(REPLACE ".hxx" "-odb.ixx" odb_ixx ${odb_files})
    string(REPLACE ".hxx" ".sql" odb_sql ${odb_files})

    if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${odb_hxx})
        add_custom_command(
            PRE_BUILD
            COMMAND odb
            ARGS -d mysql --generate-query --generate-schema --profile boost/date-try_compile
            DEPENDS ${odb_path}/${odb_file}
            OUTPUT ${CMAKE_BINARY_DIR}/${odb_cxx} ${CMAKE_BINARY_DIR}/${odb_hxx} ${CMAKE_BINARY_DIR}/${odb_ixx} ${CMAKE_BINARY_DIR}/${odb_sql}
            COMMENT "生成数据库脚本:"${odb_sql}",以及"${odb_hxx}"、"${odb_cxx}"、"${odb_ixx}
        )
    endif()
    list(APPEND pdb ${CMAKE_CURRENT_BINARY_DIR}/${odb_cxx})
endforeach()



#
# 源文件
set(project_src "")
# list(APPEND project_src speech_service.cc)  
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/ project_src)
message(${project_src})

add_executable(${project_name} ${project_src} ${pb_src} ${odb_src})

# 设置需要连接的库
target_link_libraries(${project_name} -lgflags -lspdlog -lfmt -lbrpc -lssl -lcrypto -lprotobuf -lleveldb -letcd-cpp-api -lcpprest -lcurl /usr/lib/x86_64-linux-gnu/libjsoncpp.so.19)


# test
set(test_project test_client)
project(${test_project})
set(test_files "")
list(APPEND test_files ${CMAKE_CURRENT_SOURCE_DIR}/test/test_client.cc)
add_executable(${test_project} ${test_files} ${pb_src})
target_link_libraries(${test_project} -lgtest -lgflags -lspdlog -lfmt -lbrpc -lssl -lcrypto -lprotobuf -lleveldb -letcd-cpp-api -lcpprest -lcurl /usr/lib/x86_64-linux-gnu/libjsoncpp.so.19)



#头文件搜索路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../install/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../util/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

