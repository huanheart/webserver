
#include<string>
#include<string.h>
#include<time.h>
#include<sys/time.h>         
#include<stdarg.h>// ����֧�ֿɱ��������
#include<pthread.h>

#include"log.h"


Log::Log()
{
    m_count=0;
    m_is_async=false; //��ͬ���Ķ���

}

Log::~Log()
{
    if(m_fp!=nullptr)
    {
        fclose(m_fp);        //����������
    }

}


//�첽��Ҫ�����������еĳ��ȣ�ͬ������Ҫ
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    //�������1��˵������Ҫ�첽����,��Ȼ�����ó������ж�,���Ƿ�ѡ������������
    if(max_queue_size>=1)
    {
        m_is_async=true;
        m_log_queue=new block_queue<std::string> (max_queue_size);  //��������ֱ�ӻᱻ���������������ٸ�ֵ���
        pthread_t tid;
        //flush_log_threadΪ�ص�����,�����ʾ�����߳��첽д��־
        pthread_create(&tid, NULL, flush_log_thread, NULL); 
         //�ӵ������������ʱ���߳̾��Ѿ���ȷ���ˣ���ô���е�flush_log_thread������һֱ��ִ����
        //������������һ������ָ�룬����֪�����������һ��void *�Ĳ�������ô���������ʵ�Ϳ����ɵ��ĸ�����������
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
    	//�������ã������c���Է���ַ�����Ѱ�����һ��'/'������β��֮ǰ���ַ���������
	//���û�У��򷵻�NULL
	//Ŀ�ģ����ļ������ֳ����֣��ļ����Լ�Ŀ¼��+�ļ���
    char log_full_name[256]={0,};

    if(p==nullptr)
    {
        snprintf(log_full_name,255,"%d_%02d_%02d_%s",my_tm.tm_year+1900, my_tm.tm_mon+1,my_tm.tm_mday,file_name);
    }
	else {
		//Ŀ¼�� + �ļ���
		strcpy(log_name, p + 1);   //��ֵ����log_name
		strncpy(dir_name, file_name, p - file_name + 1); //�����ǵ�ַ�ļ��㣬p - file_name + 1������'/'ǰ���ַ�����
		//ע�⣺���������windows�ø�һ�£�����Ҫ�ĳɿ�ƽ̨
		snprintf(log_full_name,255,"%s%d_%02d_%02d_%s",dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,log_name);
	}
    // snprintf����������������

    m_today=my_tm.tm_mday;
    m_fp=fopen(log_full_name,"a");
    //��ȡһ���ļ�ָ��,׷��ģʽ
	//׷��ģʽ�£�����ļ������ڣ���᳢�Դ����ļ�������ļ��Ѿ����ڣ�
	// ���ļ�ָ��ᶨλ���ļ�ĩβ��������д������ݻᱻ׷�ӵ��ļ���ĩβ�����Ḳ��ԭ�����ݡ�

    if(m_fp==nullptr) //�������ֵ��ʲô���𣿲������ڳ�ʼ�����𣬴�ʱ��m_fp�϶��ǿյ�
        return false;
    
    return true;

}

//��д��ʱ��᲻��ˢ�»��������Ա�ɹ�д��
void Log::flush(void)
{

    m_mutex.lock();  //�������Ϊʲô��Ҫ����
    fflush(m_fp);
    m_mutex.unlock();
}


void Log::write_log(int level, const char* format, ...)
{
    struct timeval now={0,0};
    gettimeofday(&now,nullptr); //��ȡ��ǰʱ���

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
        //���ÿһ����־����Ҫ�������,����ʱ����־ʱ�䣨�������ڻ��ߴ�ʱ���ļ�����̫����
        //��ô��������fclose�ر���־���������´���һ����־)
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) //everyday log   
    {
        char new_log[256]={0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16]={0};

        snprintf(tail, 16, "%d_%02d_%02d_",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday);
//snprintf��һ��C���Ա�׼���еĺ�����
//���ڸ�ʽ���ַ�����������洢��һ���ַ������У����ṩһ�ְ�ȫ���ַ�����ʽ������������ԭ��ͨ���������ģ�
        if(m_today!=my_tm.tm_mday)
        {
            snprintf(new_log,255,"%s%s%s",dir_name,tail,log_name);
            m_today=my_tm.tm_mday;
            m_count=0;
        }else{
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines); //����˵��������������µ����
        } 
        m_fp=fopen(new_log,"a");
    }

    m_mutex.unlock();

    //�����ǿɱ�����Ķ���
    va_list valst;
    va_start(valst,format);
    std::string log_str;
    m_mutex.lock();

    //д��ľ���ʱ�����ݸ�ʽ
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,    //ע�⣬��ʱ���buffֻ���Լ������Ļ�����������ͨ�������push����fputs���������������ȥ
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s ); //���͵��������У��̶�����
    int m=vsnprintf(m_buf+n,m_log_buf_size-n-1,format,valst);
    m_buf[n+m]='\n';
    m_buf[n+m+1]='\0';
    log_str=m_buf;

    m_mutex.unlock();
    if(m_is_async&&!m_log_queue->is_full())//���ѡ�����첽�汾���Ҷ���û����ʱ����ô���Ǿ�ֱ�ӷ�������У�����ֱ�����ھͿ�ʼ����
    {
        m_log_queue->push(log_str);
    }else {
        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }
    va_end(valst);
    
}






