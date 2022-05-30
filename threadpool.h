#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <exception>
#include "locker.h"
// 线程池类，定义为模板类为了代码复用，T为任务类
template<typename T>
class threadpool{
public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();

    // 添加任务
    bool append(T* request);
private:
    // 必须设置静态成员函数，避免普通成员函数导致多传入一个参数this
    static void* worker(void *);

    // 线程创建后就执行run
    void run();

    // 线程数量
    int m_thread_number;
    
    // 线程池数组 
    pthread_t * m_threads;

    // 请求队列中最多允许的，等待处理的请求数量
    int m_max_requests;

    // 请求队列
    std::list<T*> m_workqueue;

    // 互斥锁
    locker m_queuelocker;

    // 信号量来判断是否有任务需要处理
    sem m_queuestat;

    // 是否结束线程
    bool m_stop;

};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
    m_thread_number(thread_number), m_max_requests(max_requests),
    m_stop(false), m_threads(NULL){
    
    if((m_thread_number <= 0) || (max_requests) <= 0){
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }

    // 创建thread_number个线程，并将它们设置为线程脱离
    for(int i = 0;i < thread_number; ++i){
        printf("create the %dth thread\n", i);
        // this 指针将类实例传递进worker
        if(pthread_create(m_threads + i, NULL, worker, this) != 0){
            delete [] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T *request){
    // 上锁
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void *arg){
    threadpool *pool = (threadpool *) arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run() {
    while(!m_stop) {
        // 阻塞，直到信号量被释放，可以开始工作
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()){ // 判断当前是否有任务
            // 无任务
            m_queuelocker.unlock();
            continue;
        }
        // 有任务，拿走任务列表头
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if(!request) { // 是否获取到任务
            continue;
        }

        // 任务运行
        request->process();
    }
}
#endif