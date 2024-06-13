#ifndef THREADPOOL_H
#define THREADPOOL_H
#include<list>
#include<cstdio>
#include<exception>
#include<pthread.h>
#include"locker.h"
#include"sql_connection_pool.h"


//�����ģ���У����������ݽṹ��һ���Ǵ���̵߳��̳߳أ�һ���Ǵ�����������


template<typename T>
class thread_pool
{
private:
    int m_thread_number; //�̳߳��е��߳���
    int m_max_requests;       //����������������������� //������ܵ���˼����ָ�����m_max_requests���û�ͬʱ�����ҵķ�����
    pthread_t*m_threads;   //�����̳߳ص����飬���СΪm_thread_number,���̳߳�
    std::list<T*> m_workqueue; //�������
    Locker m_queuelocker; //����������еĻ�����
    Sem m_queuestat;           //�Ƿ���������Ҫ����
    Connection_pool * m_conn_pool; //���ݿ⣬ΪʲôҪ�����ݿ⣿�����������ģ�����
    int m_actor_model;        //ģ���л�
private:
    //����ǹ����߳����еĺ��������ϴӹ���������ȡ������ִ��
    static void *worker(void*arg); 
    void run();

public:
      /*thread_number���̳߳����̵߳�������max_requests������������������ġ��ȴ���������������*/
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
    if(thread_number<=0||max_requests<=0) //��֤��ʼ����������ÿͻ����ӵ�
        throw std::exception();
    m_threads=new pthread_t[m_thread_number]; //�̳߳س�ʼ��

    for(int i=0;i<thread_number;++i) //thread_numberĬ����8
    {
        if(pthread_create(m_threads+i,nullptr,worker,this)!=0) //�����߳�,�������Ӧ��m_threads+i��λ��
        //���õ�ʱ���ִ��worker����,���ĸ�������worker�����Ĳ��������ｫ���worker���������Ķ��󴫵ݸ���worker����
        {
            delete []m_threads; //˵�������߳�ʧ����
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])) //��ʼ�����̳߳ص�����̣߳��������߳̽���������̻߳���������������ֻ�������Զ��ͷ���Դ���ܰ�ȫ
        {
            delete []m_threads; //˵�������߳�ʧ��
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
bool thread_pool<T>::append(T*request,int state) //�����state��0����1������webserver��ĵ������ж���write����read
{
    m_queuelocker.lock();
    if(m_workqueue.size()>=m_max_requests) //����ĸ��������̳߳��е������������ʱ��
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state=state; //state��ʾ0�Ƕ��¼���1��д�¼�
    m_workqueue.push_back(request);

    m_queuelocker.unlock();
    m_queuestat.post(); //�ź�������һ��
    return true;

}

template<typename T>
bool thread_pool<T>::append_p(T *request)  //�첽������ӵ�������е���
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

//˼����Ϊʲô��ֱ�ӵ���run�����أ��ѵ���Ϊ�˻�ȡ���pool��
//����ΪʲôҪ�������pool�أ�
//�����run�ǻ��ɲ�ͬ���߳̽���һ�����ã�����ͬ���û������һ�����ã�����run������߼���һ���ģ���Ȼ���ֶ���̷߳���ͬһ���������У�
//��Ȼ��Ҫ���������У��ź����Ǳ�ֻ֤�����ź������󶨵��û����������Ǳ�֤�߳�ͬ��
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
        m_queuestat.wait(); //��û�м����ź�����ʱ�򣬸��߳̾�ֱ�ӵȴ�
        m_queuelocker.lock(); //��������ֹ����̹߳���ͬһ����Դ
        if(m_workqueue.empty() ) //��˫��if������Ч������һ����ֹ�̲߳�ͬ��
        {
            m_queuelocker.unlock();
            continue;
        }
        T*request=m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request)
            continue;  //����Ϊʲô����ֿյ�request����Ϊ���ܴ�ʱ��������û�����󣬵���ִ����run�����Ի��õ���)
        std::cout<<"thread dealing!"<<std::endl;
        if(m_actor_model==1) //��������ģ���л����ж������ĸ�ģ��
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
                    request->timer_flag=1;    ///����Ϊ��ʱ����Ϊ���������,��Ȼֱ�ӽ�����û�����ɾ��
                }
            }
            else {
                if(request->write() ) //д���ʱ����Ҫ�ж���ͼƬ����
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














