#include "sql_connection_pool.h"
#include "log.h"
#include <stdlib.h>
#include <pthread.h>

connection_pool::connection_pool()
{
    m_curconn = 0;
    m_freeconn = 0;
}

// singleton pattern, thread safe
connection_pool *connection_pool::GetInstance()
{
    static connection_pool conn_pool;
    return &conn_pool;
}

// initialization
void connection_pool::init(std::string url, std::string user, std::string password, std::string database_name, int port, int maxconn, int close_log)
{
    m_url = url;
    m_port = port;
    m_user = user;
    m_password = password;
    m_database_name = database_name;
    m_close_log = close_log;

    for (int i = 0; i < maxconn; i++)
    {
        MYSQL *conn = nullptr;
        conn = mysql_init(conn);

        if (conn == nullptr)
        {
            LOG_ERROR("mysql error");
            exit(1);
        }
        conn = mysql_real_connect(conn, url.c_str(), user.c_str(), password.c_str(), database_name.c_str(), port, nullptr, 0);

        if (conn == nullptr)
        {
            LOG_ERROR("mysql error");
            exit(1);
        }
        connList.push_back(conn);
        ++m_freeconn;
    }

    // initialize semaphore with maxconn;
    reserve = sync_tools::sem(m_freeconn);
    m_maxconn = m_freeconn;
}

// when a request arrives, return an available connection and update m_curconn, m_freeconn
MYSQL *connection_pool::GetConnection()
{
    MYSQL *conn = nullptr;
    if (connList.size() == 0)
        return nullptr;

    reserve.wait();
    // update shared variables betwwen threads, lock before change them
    lock.lock();

    // acquire a connection from threadpool(connList)
    conn = connList.front();
    connList.pop_front();
    ++m_freeconn;
    --m_curconn;

    lock.unlock();
    // release
    return conn;
}

// release a connection back to the threadpool
bool connection_pool::ReleaseConnection(MYSQL *conn)
{
    if (conn == nullptr)
        return false;

    lock.lock();
    connList.push_back(conn);
    ++m_freeconn;
    --m_curconn;

    lock.unlock();
    // post the semaphore after release a connection
    reserve.post();
    return true;
}

// destroy threadpool
void connection_pool::DestroyPool()
{
    lock.lock();
    if (connList.size() > 0)
    {
        std::list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it)
        {
            // MYSQL *conn = *it;
            mysql_close(*it);
        }
        m_curconn = 0;
        m_freeconn = 0;
        connList.clear();
    }
    lock.unlock();
}

// number of connections that are currently available
int connection_pool::GetFreeConnection()
{
    return this->m_freeconn;
}

// destroy threadpool with RAII mechanism
connection_pool::~connection_pool()
{
    DestroyPool();
}

// acquire and release connections by RAII mechanism
connectionRAII::connectionRAII(MYSQL **SQL, connection_pool *connPool)
{
    *SQL = connPool->GetConnection();
    connRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(connRAII);
}