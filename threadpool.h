#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include "locker.h"
#include <exception>
#include <iostream>
#include "http_conn.h"


//线程池类，定义为模板类是为了代码的复用，模板参数T是任务类
template<typename T>
class threadpool{
public:
    //默认的构造函数
    threadpool(int thread_number = 8,int max_request= 10000);
    //析构函数
    ~threadpool();
    //将任务添加到请求队列中的函数
    bool append(T* request);

private:
    //子线程运行的函数
    static void* worker(void* arg);

    //子线程运行的函数的函数
    void run();



private:
    //线程数量
    int m_thread_number;

    //线程池容器
    pthread_t * m_threads;
    
    //请求队列中允许的最大等待处理的请求数量
    int m_max_requests;

    //请求队列
    std::list<T*> m_workqueue;

    //互斥锁
    locker m_queuelock;

    //信号量判断是否有任务需要处理
    sem m_queuestat;

    //是否结束线程
    bool m_stop;
};

template<typename T>
threadpool<T>::threadpool(int thread_number,int max_request) :
m_thread_number(thread_number),m_max_requests(max_request),m_stop(false),m_threads(NULL)
{
    if((m_thread_number <= 0) || (m_max_requests <= 0)){
        throw std::exception();
    }
    //创建线程池
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    //创建m_thread_number个线程，并将它们设置为线程脱离
    for(int i = 0 ; i < m_thread_number ; ++i){
        std::cout<<"create the "<<i<<"th thread"<<std::endl;
        if(pthread_create(m_threads+i,NULL,worker,this) != 0){
            delete [] m_threads;
            throw std::exception();
        }
        //创建成功后实现线程分离
        if(pthread_detach(m_threads[i]) != 0){//这里和老师写的不一样
            delete [] m_threads;
            throw std::exception();
        }
    }

}

template<typename T>
threadpool<T>::~threadpool(){
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request){
    m_queuelock.lock();
    if(m_workqueue.size() > m_max_requests){
        m_queuelock.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelock.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg){
    threadpool * pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();
        
        m_queuelock.lock();
        if(m_workqueue.size() == 0){
            m_queuelock.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelock.unlock();

        if(!request){
            continue;
        }

        request->process();
    }
}
#endif
