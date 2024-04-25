#ifndef LOCKER_H 
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>
//线程同步机制分装类

// 1.互斥锁类
class locker{
    public:
    locker(){
        if(pthread_mutex_init(&m_mutex,NULL) != 0){
            throw std::exception();
        }
    }
    
    ~locker(){
        if(pthread_mutex_destroy(&m_mutex) != 0){
            throw std::exception();
        }
    }

    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t * get(){
        return &m_mutex;
    }

    private:
    pthread_mutex_t m_mutex;
};

// 2.条件变量类
class cond{
    public:
    cond(){
        if(pthread_cond_init(&m_cond,NULL) !=0 ){
            throw std::exception();
        }
    }

    ~cond(){
        if(pthread_cond_destroy(&m_cond) !=0 ){
            throw std::exception();
        }
    }

    //函数用于在等待条件变量的信号。它通常与互斥锁一起使用，以确保在等待过程中的线程安全
    bool wait(pthread_mutex_t * mutex){
        return pthread_cond_wait(&m_cond,mutex) == 0;
    } 

    //pthread_cond_timedwait 函数与 pthread_cond_wait 类似，
    //但它允许设置一个超时时间，即如果指定的时间到期仍未收到信号，则函数会返回
    bool timedwait(pthread_mutex_t * mutex,struct timespec * t){
        return pthread_cond_timedwait(&m_cond,mutex,t) == 0;
    }
    //函数用于向等待在条件变量上的一个线程发送信号，通知其可以继续执行
    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;
    }
    //函数用于向等待在条件变量上的所有线程发送信号，通知它们可以继续执行。
    //与 pthread_cond_signal 不同，pthread_cond_broadcast 会唤醒所有等待的线程，而不仅仅是一个
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0;
    }


    private:
    pthread_cond_t m_cond;
};

// 3.信号量类
class sem{
    public:
    sem(){
        if(sem_init(&m_sem,0,0) != 0){
            throw std::exception();
        }
    }

    sem(unsigned int num){
        if(sem_init(&m_sem,0,num) != 0){
            throw std::exception();
        }
    }
    
    ~sem(){
        if(sem_destroy(&m_sem) !=0 ){
            throw std::exception();
        }
    }

    //sem_wait 函数会将调用线程阻塞，直到信号量的值大于0，并减少信号量的值。
    //如果信号量的值为0，则调用线程将被阻塞，直到有其他线程调用 sem_post 函数来增加信号量的值为止。
    bool wait(){
        return sem_wait(&m_sem) == 0;
    }

    //sem_trywait 函数与 sem_wait 函数类似，但它是一个非阻塞的函数。它尝试对信号量进行减操作，
    //如果信号量的值大于0，则减少信号量的值并立即返回，如果信号量的值为0，则立即返回，而不会阻塞当前线程
    bool trywait(){
        return sem_trywait(&m_sem) == 0;
    }

    //sem_timedwait 函数是用于等待信号量并设定超时时间的函数。
    //它允许你等待一个信号量，在指定的时间内如果没有收到信号量的信号，则函数会返回，而不会一直阻塞等待
    bool timedwait(struct timespec * t){
        return sem_timedwait(&m_sem,t) == 0;
    }

    //sem_post 函数用于增加信号量的值，并唤醒由于等待信号量而被阻塞的线程
    bool post(){
        return sem_post(&m_sem) == 0;
    }
    
    //sem_getvalue 函数用于获取信号量的当前值,获取的值存入传出参数num中
    bool getvalue(int * num){
        return sem_getvalue(&m_sem,num) == 0;
    }



    private:
    sem_t m_sem;
};
#endif
