#ifndef LOG_H
#define LOG_H

#include<stdio.h>
#include<iostream>
#include<string>
#include<stdarg.h>
#include<pthread.h>
#include"block_queue.h"


class Log
{
private:
    Log(const Log&t)=delete;
    Log&operator=(const Log&t)=delete;
    Log();
    virtual ~Log(); //Ϊʲôʹ��������������

    void *async_write_log()
    {
        std::string single_log;
        //������������ȡ��һ����־string��д���ļ�
        while(m_log_queue->pop(single_log) )
        {
            m_mutex.lock();
            fputs(single_log.c_str(),m_fp);
            m_mutex.unlock();
        }
        return nullptr; 
    }

private:
    char dir_name[128]; //·����
    char log_name[128]; //log�ļ���
    int m_split_lines; //��־�������
    int m_log_buf_size; //��־��������С
    long long m_count; //��־������¼
    int m_today;       //��Ϊ������࣬��¼ʱ��һ��
    FILE *m_fp;        //��log���ļ�ָ��
    char *m_buf;
    block_queue<std::string> *m_log_queue; //��������
    bool m_is_async;  //�Ƿ�����ͬ��
    Locker m_mutex;
    int m_close_log; //�ر���־


public:

    //c++11�Ժ�ʹ�þֲ�������������Ҫ����
    static Log *get_instance()
    {
        static Log instance;
        return &instance;
    }

    static void * flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
        return nullptr; 
    }

    bool init(const char *file_name,int close_log,int log_buf_size=8192,int split_lines=5000000,int max_queue_size=0);

    void write_log(int level,const char*format,...);

    void flush(void);    



};

//__VA_ARGS__Ϊ����ɱ�����ĺ꣬������һ����ʽ�ַ��������������������������� LOG �걻����ʱ��__VA_ARGS__ �ᱻ�滻Ϊ���ݸ�������ж��������
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}


#endif