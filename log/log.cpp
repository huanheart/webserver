
#include<string>
#include<string.h>
#include<time.h>
#include<sys/time.h>         
#include<stdarg.h>// 用于支持可变参数函数
#include<pthread.h>

#include"log.h"


Log::Log()
{
    m_count=0;
    m_is_async=false; //流同步的东西

}

Log::~Log()
{
    if(m_fp!=nullptr)
    {
        fclose(m_fp);        //销毁这个句柄
    }

}


//异步需要设置阻塞队列的长度，同步不需要
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    //如果大于1，说明他需要异步操作,固然我们用长度来判断,他是否选择了阻塞队列
    if(max_queue_size>=1)
    {
        m_is_async=true;
        m_log_queue=new block_queue<std::string> (max_queue_size);  //匿名对象，直接会被调用析构函数，再赋值完后
        pthread_t tid;
        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL); 
         //从调用这个函数的时候，线程就已经被确立了，那么其中的flush_log_thread函数就一直被执行了
        //第三个参数是一个函数指针，可以知道这个函数有一个void *的参数，那么这个参数其实就可以由第四个参数来给到
    }

    m_close_log=close_log;
    m_log_buf_size=log_buf_size;
    m_buf=new char[m_log_buf_size];
    memset(m_buf,'\0',m_log_buf_size);
    m_split_lines=split_lines;


    time_t t=time(nullptr);
    struct tm*sys_tm=localtime(&t);
    struct tm my_tm=*sys_tm;


    const char *p=strrchr(file_name,'/'); 
    	//函数作用，从这个c语言风格字符串中寻找最后一个'/'以它结尾的之前的字符串并返回
	//如果没有，则返回NULL
	//目的，将文件名划分成两种，文件名以及目录名+文件名
    char log_full_name[256]={0,};

    if(p==nullptr)
    {
        snprintf(log_full_name,255,"%d_%02d_%02d_%s",my_tm.tm_year+1900, my_tm.tm_mon+1,my_tm.tm_mday,file_name);
    }
	else {
		//目录名 + 文件名
		strcpy(log_name, p + 1);   //赋值给到log_name
		strncpy(dir_name, file_name, p - file_name + 1); //这里是地址的计算，p - file_name + 1，即在'/'前的字符个数
		//注意：这里如果是windows得改一下，所以要改成跨平台
		snprintf(log_full_name,255,"%s%d_%02d_%02d_%s",dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,log_name);
	}
    // snprintf将结果输出到缓冲区

    m_today=my_tm.tm_mday;
    m_fp=fopen(log_full_name,"a");
    //获取一个文件指针,追加模式
	//追加模式下，如果文件不存在，则会尝试创建文件；如果文件已经存在，
	// 则文件指针会定位到文件末尾，这样新写入的内容会被追加到文件的末尾而不会覆盖原有内容。

    if(m_fp==nullptr) //这个返回值有什么用吗？不是用于初始化的吗，此时的m_fp肯定是空的
        return false;
    
    return true;

}

//在写的时候会不断刷新缓冲区，以便成功写入
void Log::flush(void)
{

    m_mutex.lock();  //考虑这个为什么需要加锁
    fflush(m_fp);
    m_mutex.unlock();
}


void Log::write_log(int level, const char* format, ...)
{
    struct timeval now={0,0};
    gettimeofday(&now,nullptr); //获取当前时间戳

    time_t t=now.tv_sec;
    struct tm *sys_tm=localtime(&t);
    struct tm my_tm=*sys_tm;
    char s[16]={0};
    switch (level)
	{
	case 0:
		strcpy(s, "[debug]:");
		break;
	case 1:
		strcpy(s, "[info]:");
		break;
	case 2:
		strcpy(s, "[warn]:");
		break;
	case 3:
		strcpy(s, "[erro]:");
		break;
	case 4:
		strcpy(s, "[info]:");
		break;
	default:
		strcpy(s, "[debug]:");
		break;
	}

    m_mutex.lock();
    m_count++;
        //针对每一个日志，都要往这边走,当此时的日志时间（当天日期或者此时的文件内容太多了
        //那么我们首先fclose关闭日志，后面重新创建一个日志)
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log   
    {
        char new_log[256]={0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16]={0};

        snprintf(tail, 16, "%d_%02d_%02d_",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday);
//snprintf是一个C语言标准库中的函数，
//用于格式化字符串并将结果存储在一个字符数组中，以提供一种安全的字符串格式化方法。它的原型通常是这样的：
        if(m_today!=my_tm.tm_mday)
        {
            snprintf(new_log,255,"%s%s%s",dir_name,tail,log_name);
            m_today=my_tm.tm_mday;
            m_count=0;
        }else{
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines); //否则说明是溢出而创建新的情况
        } 
        m_fp=fopen(new_log,"a");
    }

    m_mutex.unlock();

    //现在是可变参数的东西
    va_list valst;
    va_start(valst,format);
    std::string log_str;
    m_mutex.lock();

    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,    //注意，此时这个buff只是自己创立的缓冲区，后面通过下面的push或者fputs操作才真正输出上去
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s ); //输送到缓冲区中，固定长度
    int m=vsnprintf(m_buf+n,m_log_buf_size-n-1,format,valst);
    m_buf[n+m]='\n';
    m_buf[n+m+1]='\0';
    log_str=m_buf;

    m_mutex.unlock();
    if(m_is_async&&!m_log_queue->is_full())//如果选择了异步版本，且队列没满的时候，那么我们就直接放入队列中，否则直接现在就开始放入
    {
        m_log_queue->push(log_str);
    }else {
        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }
    va_end(valst);
    
}






