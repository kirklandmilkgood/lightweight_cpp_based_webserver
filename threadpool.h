#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "lock_wrapper.h"
#include "sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    virtual ~threadpool();
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    static void *worker(void *args); // function run by worker threads, keeping acquiring new tasks from requests queue
    void run();

    int m_thread_number;                 // number of threads in threadpool
    int m_max_requests;                  // capacity of requests queue
    pthread_t *m_threads;                // array used to implement threadpool
    std::list<T *> m_workqueue;          // requests queue
    sync_tools::mutex_lock m_queue_lock; // mutex lock used to maintain requests queue
    sync_tools::sem m_queuestate;        // semaphore used to check whether there are unsolved requests in queue
    connection_pool *m_connPool;         // mysql connection
    int m_actor_model;                   // switch of models (reactor/proactor)
};

template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests)
    : m_actor_model(actor_model), m_thread_number(thread_number), m_max_requests(max_requests), m_threads(nullptr),
      m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; i++)
    {
        if (pthread_create(m_threads + i, nullptr, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i])) // threads release resource automatically after finishing their tasks
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

template <typename T>
bool threadpool<T>::append(T *request, int state)
{
    m_queue_lock.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queue_lock.unlock();
        return false;
    }
    request->m_state = state;
    m_workqueue.push_back(request);
    m_queue_lock.unlock();
    m_queuestate.post();
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queue_lock.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queue_lock.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queue_lock.unlock();
    m_queuestate.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *args)
{
    threadpool *pool = (threadpool *)args;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while (true)
    {
        m_queuestate.wait();
        m_queue_lock.lock();
        if (m_workqueue.empty())
        {
            m_queue_lock.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queue_lock.unlock();
        if (!request)
            continue;
        if (m_actor_model == 1)
        {
            if (request->m_state == 0)
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}

#endif
