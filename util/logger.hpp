#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>

namespace chat_im::util{
    std::shared_ptr<spdlog::logger> __g_logger;

    void __init_logger__(bool mode ,const std::string& outFile_path,int logger_level){
        
        if(mode == false){
            //
            //模式为false -- debug阶段
            __g_logger = spdlog::stdout_color_mt("debug_logger");
            __g_logger->set_level(spdlog::level::level_enum::debug);
            __g_logger->flush_on(spdlog::level::level_enum::debug);

        }else{
            //
            //模式true    -- release阶段
            __g_logger = spdlog::stdout_color_mt("release_logger");
            __g_logger->set_level((spdlog::level::level_enum)logger_level);
            __g_logger->flush_on((spdlog::level::level_enum)logger_level);
        }

        __g_logger->set_pattern("%H:%M:%S [%n][%l]%v");

        

    }
}

#define DEBUG(format,...) chat_im::util::__g_logger->debug(std::string("[{:>10s}:{:<4d}]")+format,__FILE__,__LINE__,##__VA_ARGS__);
#define INFO(format,...) chat_im::util::__g_logger->info(std::string("[{:>10s}:{:<4d}]")+format,__FILE__,__LINE__,##__VA_ARGS__);
#define WARN(format,...) chat_im::util::__g_logger->warn(std::string("[{:>10s}:{:<4d}]")+format,__FILE__,__LINE__,##__VA_ARGS__);
#define ERROR(format,...) chat_im::util::__g_logger->error(std::string("[{:>10s}:{:<4d}]")+format,__FILE__,__LINE__,##__VA_ARGS__);



#endif



