#ifndef LOCKER_H
#define LOCKER_H

#include<exception>
#include<pthread.h>
#include<semaphore.h>


class Sem
{
private:
    sem_t m_sem;


public:
    Sem()
    {
        if(sem_init(&m_sem,0,0)!=0)
        {

            throw std::exception();
        }
    }

    Sem(int num)
    {
        if(sem_init(&m_sem,0,num)!=0)        //初始化信号量的值,如果出错，那么就报异常
        {
            throw std::exception();
        }
    }
    ~Sem()
    {
        sem_destroy(&m_sem);

    }
    bool wait()
    {
        //等待信号量，如果等待成功，那么就返回0
        //相当于会让信号量的值-1，注意：信号量是可以让多个线程访问同个资源，而锁只能一个线程访问一个资源
        return sem_wait(&m_sem)==0;   
    }
    bool post()
    {
        return sem_post(&m_sem)==0;
    }
};

class Locker
{
    pthread_mutex_t m_mutex;
public:
    Locker()
    {
        //为什么要使用函数？而不是自己malloc一个空间？请看笔记
        if(pthread_mutex_init(&m_mutex,NULL)!=0)
        {
            throw std::exception();
        }
    }

    ~Locker()
    {
        pthread_mutex_destroy(&m_mutex);

    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex)==0;
        //如果此时当前的这个锁没有被任何线程夺得，那么这个线程就会拿取成功，返回0.否则线程被阻塞
        //注意：拿取了锁之后，其他的线程就获取不到这个锁，那么此时需要解锁其他线程才能拿取到
    }

    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex)==0;
    }

    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

};

class Cond
{
private:
    pthread_cond_t m_cond;

public:
    Cond()
    {
        if(pthread_cond_init(&m_cond,nullptr)!=0)
        {
            throw std::exception();
        }

    }

    ~Cond()
    {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t* m_mutex)
    {
        int ret=0;
        ret=pthread_cond_wait(&m_cond,m_mutex); //在等待的同时释放掉这个互斥锁，防止发生死锁
        return ret==0;
    }

    // 有时间限制的等待
    bool time_wait(pthread_mutex_t*m_mutex,struct timespec t)
    {
        int ret=0;
        ret=pthread_cond_timedwait(&m_cond,m_mutex,&t);
        return ret==0;
    }

    //唤醒单个线程
    bool signal() 
    {
        return pthread_cond_signal(&m_cond)==0;
    }

    //唤醒多个线程
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond)==0;
    }

};

#endif

