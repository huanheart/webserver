#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

//利用循环数组实现的阻塞队列：
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
    int m_capacity;   //这个是队列的最大容量
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
        m_mutex.lock(); //为何要加锁？难道有多个线程去调用？
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
			m_cond.broadcast();     //为什么要全部唤醒？具体看第二层
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

    //pop时,如果当前队列没有元素,将会等待条件变量
    bool pop(T&item)
    {
        m_mutex.lock();
        while(m_size<=0)
        {
            //说明内部实现发生了错误，那么就直接返回，内部会将此时锁的所有权先给放掉
            if(!m_cond.wait(m_mutex.get() ) ) 
            {
                m_mutex.unlock();
                return false;
            }
            //否则，就一直循环等待，知道该线程被唤醒
        }
        //然后此时这个线程拿回来了之后，就可以pop数据了
        m_front=(m_front+1)%m_capacity;
        item=m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

    //增加了超时处理的pop操作，如果一定时间没有等待到，那么就直返回false了
    bool pop(T&item,int ms_timeout)
    {
        struct timespec t={0,0}; 
        struct timeval now={0,0}; //两个精度不同，虽然都是时间戳
        gettimeofday(&now,nullptr); //获取当前时间戳
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
        //分析一下这个，这里不懂为什么要有这个，此时都到达这里了，怎么可能会<=0呢？
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