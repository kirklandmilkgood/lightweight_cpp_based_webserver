#ifndef HTTP_CONNECTION_H
#define HTTP_CONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include "lock_wrapper.h"
#include "sql_connection_pool.h"
#include "log.h"

class http_connection
{
public:
    // length of file name
    static const int FILENAME_LEN = 200;
    // size of reading buffer
    static const int READ_BUFFER_SIZE = 2048;
    // size of writting buffer
    static const int WRITE_BUFFER_SIZE = 1024;
    // request method
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    // state of master state machime
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, // parse request line
        CHECK_STATE_HEADER,          // parse request header
        CHECK_STATE_CONTENT          // parse request content (usually used in POST request message)
    };
    // outcome of http message parsing
    enum HTTP_CODE
    {
        NO_REQUEST,  // incomplete request, should continue to read the request message
        GET_REQUEST, // recieve a complete request
        BAD_REQUEST, // have syntax errors in request message
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    // state of slave state machine
    enum LINE_STATUS
    {
        LINE_OK = 0, // read an complete line
        LINE_BAD,    // have syntax error in request message
        LINE_OPEN    // read an incomplete line
    };

    http_connection(){};
    ~http_connection(){};
    // initialize socket address, and call private initialization method (init())
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, std::string user, std::string password, std::string sql_name);
    // close http connection
    void close_connection(bool real_close = true);
    void process();
    // receive message sent from browser
    bool read_once();
    // write response message
    bool write();
    sockaddr_in *get_address() { return &m_address; }
    // use connectionpool to initialize tables in database
    void initmysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;

    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state; // 0:read, 1:write

private:
    void init();
    // read message in m_read_buf and process
    HTTP_CODE process_read();
    // write response message into m_write_buf
    bool process_write(HTTP_CODE ret);
    // parse request line of request message with master S-M
    HTTP_CODE parse_request_line(char *text);
    // parse header of request message with master S-M
    HTTP_CODE parse_headers(char *text);
    // parse content of request message with master S-M
    HTTP_CODE parse_content(char *text);
    // produce response message
    HTTP_CODE do_request();
    // m_start_line are the chars that have been parsed, and get_line() is used to move the pointer backward to the raw chars
    char *get_line() { return m_read_buf + m_start_line; };
    // read a line from slave S-M and determine it is from which part of message
    LINE_STATUS parse_line();
    void unmap();
    // following 8 methods is used to produce response message in accordance with designated framework of message
    //, which are all called by do_request()
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

    int m_sockfd;
    sockaddr_in m_address;
    // save data from request message
    char m_read_buf[READ_BUFFER_SIZE];
    // the  next index of last char in m_read_buf
    long m_read_idx;
    // the position where m_read_buf is read
    long m_checked_idx;
    // the number of chars that have been parsed
    int m_start_line;
    // save data of response message
    char m_write_buf[WRITE_BUFFER_SIZE];
    // indicate the length of data in m_write_buf
    int m_write_idx;
    // state of master S-M
    CHECK_STATE m_check_state;
    // request method
    METHOD m_method;
    // following 6 variables is related to the outcome of request message parsing
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    long m_content_length;
    bool m_linger;
    // file address of the file in the server
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;
    // save the data from header of request message
    char *m_string;
    // bytes of chars that haven't been sent
    int bytes_to_send;
    // bytes of chars that have been sent
    int bytes_have_send;
    char *doc_root;

    std::map<std::string, std::string> m_users;
    int m_TRIGMode;
    int m_close_log;
    char sql_user[100];
    char sql_password[100];
    char sql_name[100];
};
#endif