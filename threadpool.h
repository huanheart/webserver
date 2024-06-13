#ifndef THREADPOOL_H
#define THREADPOOL_H
#include<list>
#include<cstdio>
#include<exception>
#include<pthread.h>
#include"locker.h"
#include"sql_connection_pool.h"


//在这个模块中，有两个数据结构，一个是存放线程的线程池，一个是存放请求的链表


template<typename T>
class thread_pool
{
private:
    int m_thread_number; //线程池中的线程数
    int m_max_requests;       //请求队列中允许的最大请求数 //这个可能的意思就是指最多有m_max_requests的用户同时访问我的服务器
    pthread_t*m_threads;   //描述线程池的数组，其大小为m_thread_number,即线程池
    std::list<T*> m_workqueue; //请求队列
    Locker m_queuelocker; //保护请求队列的互斥锁
    Sem m_queuestat;           //是否有任务需要处理
    Connection_pool * m_conn_pool; //数据库，为什么要有数据库？这个用来干嘛的？？？
    int m_actor_model;        //模型切换
private:
    //这个是工作线程运行的函数，不断从工作队列中取出任务并执行
    static void *worker(void*arg); 
    void run();

public:
      /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
      thread_pool(int actor_model, Connection_pool *connPool, int thread_number = 8, int max_request = 10000);
      ~thread_pool();
      bool append(T*request,int state);
      bool append_p(T*request);



};

template<typename T>
thread_pool<T>::thread_pool(int actor_model, Connection_pool *connPool, int thread_number, int max_requests) : 
    m_actor_model(actor_model),m_thread_number(thread_number),
     m_max_requests(max_requests), m_threads(NULL),m_conn_pool(connPool)
{
    if(thread_number<=0||max_requests<=0) //保证初始化必须可以让客户连接的
        throw std::exception();
    m_threads=new pthread_t[m_thread_number]; //线程池初始化

    for(int i=0;i<thread_number;++i) //thread_number默认是8
    {
        if(pthread_create(m_threads+i,nullptr,worker,this)!=0) //创建线程,放入道对应的m_threads+i的位置
        //调用的时候会执行worker函数,第四个参数是worker函数的参数，这里将这个worker函数所属的对象传递给了worker函数
        {
            delete []m_threads; //说明创建线程失败了
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])) //开始调用线程池的这个线程，可能主线程结束了这个线程还会继续，但是这种会帮助你自动释放资源，很安全
        {
            delete []m_threads; //说明开启线程失败
            throw std::exception();
        }

    }

}


template<typename T>
thread_pool<T>::~thread_pool()
{
    delete []m_threads;
}

template<typename T>
bool thread_pool<T>::append(T*request,int state) //这里的state是0还是1来是由webserver层的调用来判断是write还是read
{
    m_queuelocker.lock();
    if(m_workqueue.size()>=m_max_requests) //请求的个数大于线程池中的最大请求数的时候
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state=state; //state表示0是读事件，1是写事件
    m_workqueue.push_back(request);

    m_queuelocker.unlock();
    m_queuestat.post(); //信号量增加一个
    return true;

}

template<typename T>
bool thread_pool<T>::append_p(T *request)  //异步处理添加到请求队列当中
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

//思考，为什么不直接调用run函数呢？难道是为了获取这个pool？
//而且为什么要返还这个pool呢？
//这里的run是会由不同的线程进行一个调用（即不同的用户会进行一个调用，但是run里面的逻辑是一样的，固然出现多个线程访问同一个阻塞队列）
//固然需要加锁，其中，信号量是保证只能有信号量所绑定的用户数量，锁是保证线程同步
template<typename T>
void* thread_pool<T>::worker(void *arg) 
{
    thread_pool *pool =(thread_pool*) arg;
    pool->run();
    return pool; 
}



template<typename T>
void thread_pool<T>::run()
{
    while(true)
    {
        m_queuestat.wait(); //当没有计数信号量的时候，该线程就直接等待
        m_queuelocker.lock(); //加锁，防止多个线程共享同一个资源
        if(m_workqueue.empty() ) //，双重if的类似效果，进一个防止线程不同步
        {
            m_queuelocker.unlock();
            continue;
        }
        T*request=m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request)
            continue;  //分析为什么会出现空的request（因为可能此时队列里面没有请求，但是执行了run，所以会拿到空)
        std::cout<<"thread dealing!"<<std::endl;
        if(m_actor_model==1) //这玩意是模型切换，判断你是哪个模型
        {
            if(request->m_state==0)
            {
                if(request->read_once() )
                {
                    request->improv=1;
                    connectionRAII mysqlcon(&request->mysql,m_conn_pool); //
                    request->process();
                }else {
                    request->improv=1;
                    request->timer_flag=1;    ///设置为超时，因为这个错误了,固然直接将这个用户进行删除
                }
            }
            else {
                if(request->write() ) //写入的时候，需要判断是图片还是
                {
                    request->improv=1;
                }else {
                    request->improv=1;
                    request->timer_flag=1;
                }
            }

        }else {
            connectionRAII mysqlcon(&request->mysql,m_conn_pool);
            request->process();
        }    
    }

}




#endif














