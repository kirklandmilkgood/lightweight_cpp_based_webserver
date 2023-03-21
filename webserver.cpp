#include "webserver.h"

webserver::webserver()
{
    // http-connection task queue, used by worker threads
    users = new http_connection[MAX_FD];
    // root folder path
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    // timer
    users_timer = new client_data[MAX_FD];
}

webserver::~webserver()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void webserver::init(int port, std::string user, std::string password, std::string database_name, int log_write, int opt_linger, int TRIGMode, int sql_num, int thread_num,
                     int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_password = password;
    m_database_name = database_name;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_opt_linger = opt_linger;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;
    m_actor_model = actor_model;
}

void webserver::trig_mode()
{
    // LT + LT
    if (m_TRIGMode == 0)
    {
        m_listen_TRIGMode = 0;
        m_conn_TRIGMODE = 0;
    }

    // LT + ET
    else if (m_TRIGMode == 1)
    {
        m_listen_TRIGMode = 0;
        m_conn_TRIGMODE = 1;
    }

    // ET + LT
    else if (m_TRIGMode == 2)
    {
        m_listen_TRIGMode = 1;
        m_conn_TRIGMODE = 0;
    }

    // ET + ET
    else if (m_TRIGMode == 3)
    {
        m_listen_TRIGMode = 1;
        m_conn_TRIGMODE = 1;
    }
}

void webserver::log_write()
{
    if (m_close_log == 0)
    {
        // initialize log
        if (m_close_log == 1)
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

void webserver::sql_pool()
{
    // initialize connection pool
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_password, m_database_name, 3306, m_sql_num, m_close_log);

    // initialize mysql result table
    users->initmysql_result(m_connPool);
}

void webserver::thread_pool()
{
    // threadpool
    m_pool = new threadpool<http_connection>(m_actor_model, m_connPool, m_thread_num);
}

void webserver::event_listen()
{
    // socket programming
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd > 0);

    // linger
    if (m_opt_linger == 0)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (m_opt_linger == 1)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);
    utils.init(TIMESLOT);

    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    utils.addfd(m_epollfd, m_listenfd, false, m_listen_TRIGMode);
    http_connection::m_epollfd = m_epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    utils.add_signal(SIGPIPE, SIG_IGN);
    utils.add_signal(SIGALRM, utils.signal_handler, false);
    utils.add_signal(SIGTERM, utils.signal_handler, false);

    alarm(TIMESLOT);

    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

void webserver::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_conn_TRIGMODE, m_close_log, m_user, m_password, m_database_name);
    // initialize client data
    // create timers, set callback function, timing and bind user data, append timers to the list
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->callback_func = callback_func;
    time_t curr = time(nullptr);
    timer->expire = curr + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_list.add_timer(timer);
}

// if there is data transfered in through this connection then move the timing of timer 3 units forward
// and adjust the position of the timer in the list
void webserver::adjust_timer(util_timer *timer)
{
    time_t curr = time(nullptr);
    timer->expire = curr + 3 * TIMESLOT;
    utils.m_timer_list.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

void webserver::deal_timer(util_timer *timer, int sockfd)
{
    timer->callback_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_list.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool webserver::deal_client_data()
{
    struct sockaddr_in client_address;
    socklen_t client_addr_length = sizeof(client_address);
    if (m_listen_TRIGMode == 0)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addr_length);
        if (connfd < 0)
        {
            LOG_ERROR("%s: errno is %d", "accept error", errno);
            return false;
        }
        if (http_connection::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }
    else
    {
        while (true)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addr_length);
            if (connfd < 0)
            {
                LOG_ERROR("%s: errno is %d", "accepr error", errno);
                break;
            }
            if (http_connection::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool webserver::deal_with_signal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int signal;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
        return false;
    else if (ret == 0)
        return false;
    else
    {
        for (int i = 0; i < ret; i++)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void webserver::deal_with_read(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    // reactor model
    if (m_actor_model == 1)
    {
        if (timer)
            adjust_timer(timer);
        // if a read event is detected, put it into the request queue
        m_pool->append(users + sockfd, 0);

        while (true)
        {
            if (users[sockfd].improv == 1)
            {
                deal_timer(timer, sockfd);
                users[sockfd].timer_flag = 0;
            }
            users[sockfd].improv = 0;
            break;
        }
    }
    else
    {
        // proactor model
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client (%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            // if a read event is detected, put it into the request queue
            m_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void webserver::deal_with_write(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    // reactor
    if (m_TRIGMode == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        m_pool->append(users + sockfd, 1);
        while (true)
        {
            if (users[sockfd].improv == 1)
            {
                if (users[sockfd].timer_flag == 1)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        // proactor
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client (%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void webserver::event_loop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            // deal with new client connection
            if (sockfd == m_listenfd)
            {
                bool flag = deal_client_data();
                if (flag == false)
                    continue;
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // close connection from server side, remove corresponding timer
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            // deal with signal
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = deal_with_signal(timeout, stop_server);
                if (flag == false)
                    LOG_ERROR("%s", "deal with client data failure");
            }
            // deal with data received from client side
            else if (events[i].events & EPOLLIN)
            {
                deal_with_read(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                deal_with_write(sockfd);
            }
        }
        if (timeout)
        {
            utils.timer_handler();
            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }
}