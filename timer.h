#ifndef TIMER_H
#define TIMER_H

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
#include <time.h>
#include "log.h"

class util_timer;
struct client_data
{
    sockaddr_in address; // socket address of client
    int sockfd;          // socket fd
    util_timer *timer;   // timer pointer
};

class util_timer
{
public:
    util_timer() : prev(nullptr), next(nullptr) {}
    time_t expire;
    void (*callback_func)(client_data *); // callback function
    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

class sort_timer_list
{
public:
    sort_timer_list();
    ~sort_timer_list();

    void add_timer(util_timer *timer);    // add a timer to the list
    void adjust_timer(util_timer *timer); // adjust the position of a designated timer in the list
    void del_timer(util_timer *timer);    // delete a timer in the list
    void tick();                          // task handler function called whenever SIGNALRM signal is triggered

private:
    void add_timer(util_timer *timer, util_timer *list_head);
    util_timer *head;
    util_timer *tail;
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    // signal handler function
    static void signal_handler(int signal);
    void init(int time_slot);
    // set fd in nonblocking mode
    int setnonblocking(int fd);
    // register a read event to kernel(et mode, epolloneshot)
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
    // signal-action function
    void add_signal(int signal, void (*handler)(int), bool restart = true);
    // reset timer to trigger SIGNALRM signal again
    void timer_handler();
    void show_error(int connfd, const char *info);

    static int *u_pipefd;
    sort_timer_list m_timer_list;
    static int u_epollfd;
    int m_time_slot;
};
void callback_func(client_data *user_data);

#endif