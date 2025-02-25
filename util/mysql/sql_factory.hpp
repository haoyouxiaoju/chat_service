#ifndef SQL_FACTORY_HPP
#define SQL_FACTORY_HPP

#include <iostream>
#include <string>
#include <odb/database.hxx>
#include <odb/mysql/database.hxx>


namespace chat_im::util
{
class ODBFactory{
public:
    static std::shared_ptr<odb::core::database> create(
        const std::string& user,
        const std::string& password,
        const std::string& host,
        const std::string& db,
        const std::string& socket,
        int port,
        int conn_pool_count
    )
    {
        std::unique_ptr<odb::mysql::connection_pool_factory> cpf(   \
            new odb::mysql::connection_pool_factory(conn_pool_count,0));
        std::shared_ptr<odb::mysql::core::database> res             \
            = std::make_shared<odb::mysql::database>(user,  \
                    password,db,host,port,"",socket,0,std::move(cpf));
        return res;
    }

};

    
} // namespace util



#endif