#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>

#include "threadpool.h"
#include "http_connection.h"
#include "timer.h"

const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5;

class webserver
{
public:
    webserver();
    ~webserver();
    void init(int port, std::string user, std::string password, std::string database_name, int log_write, int opt_linger, int TRIGMode, int sql_num, int thread_num,
              int close_log, int actor_model);
    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void event_listen();
    void event_loop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);
    bool deal_client_data();
    bool deal_with_signal(bool &timeout, bool &stop_server);
    void deal_with_read(int sockfd);
    void deal_with_write(int sockfd);

    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actor_model;
    int m_pipefd[2];
    int m_epollfd;
    http_connection *users;

    connection_pool *m_connPool;
    std::string m_user;
    std::string m_password;
    std::string m_database_name;
    int m_sql_num;

    threadpool<http_connection> *m_pool;
    int m_thread_num;

    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_opt_linger;
    int m_TRIGMode;
    int m_listen_TRIGMode;
    int m_conn_TRIGMODE;

    client_data *users_timer;
    Utils utils;
};
#endif