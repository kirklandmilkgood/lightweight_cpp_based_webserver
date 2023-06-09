#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "log.h"

Log::Log()
{
    m_count = 0;
    m_is_async = false;
}

Log::~Log()
{
    if (m_fp != nullptr)
        fclose(m_fp);
}

bool Log::init(const char *file_name, int close_log, int log_buf_size, int spilt_lines, int max_queue_size)
{
    if (max_queue_size >= 1)
    {
        m_is_async = true;
        m_log_queue = new block_queue<std::string>(max_queue_size);
        pthread_t tid;
        // "flush_log_thread" is a callback function used to create a thread that writes async log
        pthread_create(&tid, nullptr, flush_log_thread, nullptr);
    }

    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    m_spilt_lines = spilt_lines;

    time_t t = time(nullptr);
    struct tm *sys_time = localtime(&t);
    struct tm my_time = *sys_time;

    const char *p = strchr(file_name, '/');
    char log_full_name[256] = {0};

    if (p == nullptr)
    {
        snprintf(log_full_name, 255, "%d_%02d_%02d_%s", my_time.tm_year + 1900, my_time.tm_mon + 1, my_time.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name, my_time.tm_year + 1900, my_time.tm_mon + 1, my_time.tm_mday, log_name);
    }

    m_today = my_time.tm_mday;
    m_fp = fopen(log_full_name, "a");
    if (m_fp == nullptr)
        return false;
    return true;
}

void Log::write_log(int level, const char *format, ...)
{
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm *sys_time = localtime(&t);
    struct tm my_time = *sys_time;
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[error]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    m_mutex.lock();
    m_count++;

    if (m_today != my_time.tm_mday || m_count % m_spilt_lines == 0)
    {
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_time.tm_year + 1900, my_time.tm_mon + 1, my_time.tm_mday);
        if (m_today != my_time.tm_mday)
        {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
        }
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();
    va_list valst;
    va_start(valst, format);

    std::string log_str;
    m_mutex.lock();

    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d%02d:%02d.%06ld %s", my_time.tm_year + 1900, my_time.tm_mon + 1,
                     my_time.tm_mday, my_time.tm_hour, my_time.tm_min, my_time.tm_sec, now.tv_usec, s);

    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;

    m_mutex.unlock();
    if (m_is_async && !m_log_queue->full())
    {
        m_log_queue->push(log_str);
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }

    va_end(valst);
}

void Log::flush()
{
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}