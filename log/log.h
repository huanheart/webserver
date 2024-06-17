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
    virtual ~Log(); //为什么使用虚析构函数？

    void *async_write_log()
    {
        std::string single_log;
        //从阻塞队列中取出一个日志string，写入文件
        while(m_log_queue->pop(single_log) )
        {
            m_mutex.lock();
            fputs(single_log.c_str(),m_fp);
            m_mutex.unlock();
        }
        return nullptr; 
    }

private:
    char dir_name[128]; //路径名
    char log_name[128]; //log文件名
    int m_split_lines; //日志最大行数
    int m_log_buf_size; //日志缓冲区大小
    long long m_count; //日志行数记录
    int m_today;       //因为按天分类，记录时哪一天
    FILE *m_fp;        //打开log的文件指针
    char *m_buf;
    block_queue<std::string> *m_log_queue; //阻塞队列
    bool m_is_async;  //是否开启流同步
    Locker m_mutex;
    int m_close_log; //关闭日志


public:

    //c++11以后，使用局部变量懒汉不需要加锁
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

//__VA_ARGS__为处理可变参数的宏，它接受一个格式字符串和任意数量的其他参数。在 LOG 宏被调用时，__VA_ARGS__ 会被替换为传递给宏的所有额外参数。
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}


#endif