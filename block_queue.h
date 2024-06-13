#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

//����ѭ������ʵ�ֵ��������У�
#include<iostream>
#include<stdlib.h>
#include<pthread.h>
#include<sys/time.h>
#include"locker.h"

template<class T>
class block_queue
{

private:
    Locker m_mutex;
    Cond m_cond;
    T* m_array;
    int m_size;
    int m_capacity;   //����Ƕ��е��������
    int m_front;
    int m_back;
public:
    block_queue(int capacity=1000)
    {
        if(capacity<=0)
        {
            exit(-1);
        }
        m_capacity=capacity;
        m_array=new T[m_capacity];
        m_size=0;
        m_front=-1;
        m_back=-1;
    }

    ~block_queue()
    {
        m_mutex.lock(); //Ϊ��Ҫ�������ѵ��ж���߳�ȥ���ã�
        if(m_array!=nullptr)
            delete[]m_array;
        m_mutex.unlock();
    }

    void clear()
    {
		m_mutex.lock();
		m_size = 0;
		m_front = -1;
		m_back = -1;
		m_mutex.unlock();
    }

    bool is_full()
    {
        m_mutex.lock();
		if (m_size >= m_capacity)
		{
			m_mutex.unlock();
			return true;
		}
		m_mutex.unlock();
		return false;
    }

	bool is_empty()
	{
		m_mutex.lock();
		if (m_size == 0)
		{
			m_mutex.unlock();
			return true;
		}
		m_mutex.unlock();
		return false;
	}

    bool get_front(T & value)
	{
		m_mutex.lock();
		if (m_size == 0) {
			m_mutex.unlock();
			return false;
		}
		value = m_array[m_front];
		m_mutex.unlock();
		return true;
	}


    bool get_back(T& value)
	{
		m_mutex.lock();
		if (m_size == 0) {
			m_mutex.unlock();
			return false;
		}
		value = m_array[m_back];
		m_mutex.unlock();
		return true;
	}

	int get_size()
	{
		m_mutex.lock();
		int tmp = m_size;
		m_mutex.unlock();
		return tmp;
	}

	int get_m_capacity()
	{
		m_mutex.lock();
		int tmp = m_capacity;
		m_mutex.unlock();
		return tmp;
	}



	bool push(const T& item)
	{
		m_mutex.lock();
		if (m_size >= m_capacity)
		{
			m_cond.broadcast();     //ΪʲôҪȫ�����ѣ����忴�ڶ���
			m_mutex.unlock();
			return false;
		}
		m_back = (m_back + 1) % m_capacity;
		m_array[m_back] = item;
		m_size++;
		m_cond.broadcast();
		m_mutex.unlock();
		return true;
	}

    //popʱ,�����ǰ����û��Ԫ��,����ȴ���������
    bool pop(T&item)
    {
        m_mutex.lock();
        while(m_size<=0)
        {
            //˵���ڲ�ʵ�ַ����˴�����ô��ֱ�ӷ��أ��ڲ��Ὣ��ʱ��������Ȩ�ȸ��ŵ�
            if(!m_cond.wait(m_mutex.get() ) ) 
            {
                m_mutex.unlock();
                return false;
            }
            //���򣬾�һֱѭ���ȴ���֪�����̱߳�����
        }
        //Ȼ���ʱ����߳��û�����֮�󣬾Ϳ���pop������
        m_front=(m_front+1)%m_capacity;
        item=m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

    //�����˳�ʱ�����pop���������һ��ʱ��û�еȴ�������ô��ֱ����false��
    bool pop(T&item,int ms_timeout)
    {
        struct timespec t={0,0}; 
        struct timeval now={0,0}; //�������Ȳ�ͬ����Ȼ����ʱ���
        gettimeofday(&now,nullptr); //��ȡ��ǰʱ���
        m_mutex.lock();
        if(m_size<=0)
        {
            t.tv_sec=now.tv_sec+ms_timeout/1000;
            t.tv_nsec=(ms_timeout%1000)*1000;
            if(!m_cond.time_wait(m_mutex.get(),t) )
            {
                m_mutex.unlock();
                return false;
            }

        }
        //����һ����������ﲻ��ΪʲôҪ���������ʱ�����������ˣ���ô���ܻ�<=0�أ�
        if(m_size<=0)
        {
            m_mutex.lock();
            return false;
        }

        m_front=(m_front+1)%m_capacity;
        item=m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

};

#endif