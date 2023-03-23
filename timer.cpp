#include "timer.h"
#include "http_connection.h"

sort_timer_list::sort_timer_list()
{
    head = nullptr;
    tail = nullptr;
}

sort_timer_list::~sort_timer_list()
{
    util_timer *tmp = head;
    while (tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void sort_timer_list::add_timer(util_timer *timer)
{
    if (!timer)
        return;
    // if sort_timer_list is empty:
    if (!head)
    {
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

void sort_timer_list::adjust_timer(util_timer *timer)
{
    if (!timer)
        return;
    util_timer *tmp = timer->next;
    if (!tmp || (timer->expire < tmp->expire))
        return;
    if (timer == head)
    {
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void sort_timer_list::del_timer(util_timer *timer)
{
    if (!timer)
        return;
    if ((timer == head) && (timer == tail))
    {
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }
    if (timer == head)
    {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_list::tick()
{
    if (!head)
        return;
    // acquire current time
    time_t current = time(nullptr);
    util_timer *tmp = head;
    // iterate throughout the entire timer list to find which timer(s) is/are expired.
    while (tmp)
    {
        if (current < tmp->expire)
            break;
        tmp->callback_func(tmp->user_data); // call callback function when a timer is expired
        head = tmp->next;
        if (head)
            head->prev = nullptr;
        delete tmp;
        tmp = head; // move to next timer in the list
    }
}

// privte add_timer function
void sort_timer_list::add_timer(util_timer *timer, util_timer *list_head)
{
    util_timer *prev = list_head;
    util_timer *tmp = prev->next;
    // iterate all the elements after current element in the list
    // find the proper position that current timer will be placed in
    while (tmp)
    {
        if (timer->expire < tmp->expire) // insertion of current timer
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    // iterate to the end of the list, current timer will be put in the tail of the list
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = nullptr;
        tail = timer;
    }
}

void Utils::init(int time_slot)
{
    m_time_slot = time_slot;
}

// set fd in nonblocking mode
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// register a read event to kernel
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;
    // ET mode
    if (TRIGMode == 1)
    {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }
    // LT mode
    else
        event.events = EPOLLIN | EPOLLRDHUP;
    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// signal-action function (just send the signal value, no extra procedure)
void Utils::signal_handler(int signal)
{
    // save original errno in order to ensure the possibility of re-entering the function
    int save_errno = errno;
    int message = signal;
    // write signal value into the writing side of the pipeline, and set the data flow in form of char
    send(u_pipefd[1], (char *)&message, 1, 0);
    errno = save_errno;
}

// set signal function (specifically handles SLGTERM and SICGALRM signals)
void Utils::add_signal(int signal, void (*handler)(int), bool restart)
{
    // sigaction struction uesd to set signal handler, signal mask, signal flags...
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    // handler is a function pointer pointing to the designated signal handler function
    sa.sa_handler = handler;
    // SA_RESTART: the flag indicates that the syscall interrupted by signals can be restarted automatically
    if (restart)
        sa.sa_flags |= SA_RESTART;
    // add all signals to a signal set
    sigfillset(&sa.sa_mask);
    assert(sigaction(signal, &sa, nullptr) != -1);
}

// reset the timer in order to trigger SIGALRM signal again
void Utils::timer_handler()
{
    m_timer_list.tick();
    // alarm function will trigger SIGALRM signal periodically according to the time interval developer set
    alarm(m_time_slot);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void callback_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_connection::m_user_count--;
}
