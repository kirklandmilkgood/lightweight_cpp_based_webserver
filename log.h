#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

class Log
{
public:
    // singleton pattern
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
    }
    // optional arguments: log file, log buffer capacity, max lines of records in log file, max capacity of log queue
    bool init(const char *filename, int close_log, int log_buf_size = 8192, int split_lines = 5e6, int max_queue_size = 0);

    void write_log(int level, const char *format, ...);

    void flush(void);

private:
    Log();
    virtual ~Log();
    void *async_write_log()
    {
        std::string single_log;
        // get a log string from queue and write into log file
        while (m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }

    char dir_name[128]; // file path
    char log_name[128]; // log file name
    int m_spilt_lines;  // max lines of records in log file
    int m_log_buf_size; // log buffer capacity
    long long m_count;  // existing lines of records in log file
    int m_today;
    FILE *m_fp;                            // log file pointer
    char *m_buf;                           // buffer array
    block_queue<std::string> *m_log_queue; // circular queue for log writing (blocking queue)
    bool m_is_async;                       // flag (sync or async)
    sync_tools::mutex_lock m_mutex;
    int m_close_log; // whether to stop log service
};

#define LOG_DEBUG(format, ...)                                    \
    if (m_close_log == 0)                                         \
    {                                                             \
        Log::get_instance()->write_log(0, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_INFO(format, ...)                                     \
    if (m_close_log == 0)                                         \
    {                                                             \
        Log::get_instance()->write_log(1, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#define LOG_WARN(format, ...)                                     \
    if (m_close_log == 0)                                         \
    {                                                             \
        Log::get_instance()->write_log(2, format, ##__VA__ARGS_); \
        Log::get_instance()->flush();                             \
    }
#define LOG_ERROR(format, ...)                                    \
    if (m_close_log = 0)                                          \
    {                                                             \
        Log::get_instance()->write_log(3, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                             \
    }
#endif
