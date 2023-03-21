#ifndef CONNECTION_POOL
#define CONNECTION_POOL

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "lock_wrapper.h"

class connection_pool
{
public:
    MYSQL *GetConnection();                    // get mysql connection
    bool ReleaseConnection(MYSQL *connection); // release connection
    int GetFreeConnection();                   // get connection
    void DestroyPool();                        // destroy all connections

    // singleton pattern
    static connection_pool *GetInstance();
    void init(std::string url, std::string user, std::string password, std::string database_name, int port, int maxconn, int close_log);

    std::string m_url;           // host address
    std::string m_port;          // port number of mysql
    std::string m_user;          // username used to connect mysql
    std::string m_database_name; // name of targeted database
    std::string m_password;      // password used to connect mysql
    int m_close_log;             // var that decides whether to write records into log

private:
    connection_pool();
    ~connection_pool();

    int m_maxconn;  // max connection number
    int m_curconn;  // number of connections that have been used
    int m_freeconn; // number of connections that are currently available
    sync_tools::mutex_lock lock;
    std::list<MYSQL *> connList; // connection pool built with a list container
    sync_tools::sem reserve;
};

class connectionRAII
{
public:
    connectionRAII(MYSQL **con, connection_pool *conn_pool);
    ~connectionRAII();

private:
    MYSQL *connRAII;
    connection_pool *poolRAII;
};

#endif