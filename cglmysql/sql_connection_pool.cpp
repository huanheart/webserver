#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include "sql_connection_pool.h"


//c++11�Ժ󣬾ֲ���̬��������Ҫ����
Connection_pool* Connection_pool::get_instance()
{
    static Connection_pool conn_pool;
    return &conn_pool;
}

void Connection_pool::init(std::string url, std::string user,std::string passwd, std::string data_base_name,int port, int max_conn, int close_log)
{
	m_url = url; //������ַ
	m_port = port;   //�˿ں���Щ
	m_user = user;
	m_passwd = passwd;
	m_databasename = data_base_name;
	m_close_log = close_log;   //��־����
    //�����������ӳط�����
    for(int i=0;i<max_conn;i++)
    {        
        MYSQL*con=nullptr;
        con=mysql_init(con);  //��ʼ�����ѣ���û������
        if(con==nullptr)
        {
            LOG_ERROR("MYSQL ERROR"); //ֱ��������������־����������ʼ����ʱ�������,�����ж˿ں���Щ��ԭ��
            exit(1);
        }
        //��þ���������õ�����ռ�
       con = mysql_real_connect(con, url.c_str(), user.c_str(), passwd.c_str(), data_base_name.c_str(), port, nullptr, 0);
       if(con==nullptr)
       {
            LOG_ERROR("MYSQL ERROR");
            exit(1);
       }
       conn_list.push_back(con); //����һ������
       ++m_free_conn; //��Ϊ��û���õ��ողŷŽ�ȥ��������ӣ���Ȼ�ڳ�����������Ӷ���һ��

    }

    reserve=Sem(m_free_conn); //�ź�����ʼ�����������������������������������
    m_max_conn=m_free_conn; //m_free_conn����͸տ�ʼ�������������һ���ģ���Ϊ���ڲ���������еĸ�++��

}


//��������ʱ�������ݿ����ӳ��з���һ���������ӣ�����ʹ�úͿ���������
MYSQL * Connection_pool::get_connection()
{
    MYSQL *con=nullptr;
    if(conn_list.size()==0)
        return nullptr;
    reserve.wait(); //�ź���ȥ�����ȴ��������û����ػ���������

    lock.lock();
    con=conn_list.front();
    conn_list.pop_front();
    --m_free_conn;
    ++m_cur_conn;
    lock.unlock();
    return con;
}


//�ͷŵ�ǰ����
bool Connection_pool::release_connection(MYSQL*con)
{
    if(con==nullptr)
        return false;
    lock.lock();
    
    conn_list.push_back(con); //�ع鵽������
    ++m_free_conn;
    --m_cur_conn;
    lock.unlock();

    reserve.post(); 
    //ʹ������ź���+1����Ϊ��ʱ��϶����ж���̷߳�������������
    //��Ȼ���ͷź����ӵĺ����϶��������̵߳��õ�,������ͻ��˷���һ�����������̣߳��������Ҫ��һ���߳�Ϊ�ͻ�ȥ������?
    //��ʵû�б�Ҫ��,����ʹ��Э��
    return true;

}

//�������ݿ����ӳص���������

void Connection_pool::destroy_pool()
{
    lock.lock();
    if(conn_list.size()>0)
    {
        std::list<MYSQL*>::iterator it;   //�ڲ�ά��Ӧ���Ǹ�����ָ���ˣ�����ģ���Ӧ������ά��һ��ָ�������ָ��
        for(it=conn_list.begin();it!=conn_list.end();++it)
        {
            MYSQL*con=*it;
            mysql_close(con);
        }
        m_cur_conn=0;
        m_free_conn=0;
        conn_list.clear();
    }
    lock.unlock();
}

int Connection_pool::get_free_conn()
{
    return this->m_free_conn;
}

Connection_pool::~Connection_pool() //����rall��˼��
{
    destroy_pool();
}


connectionRAII::connectionRAII(MYSQL**con,Connection_pool*conn_pool)
{
    *con=conn_pool->get_connection();
    conRAII=*con;
    poolRAII=conn_pool;

}

connectionRAII::~connectionRAII()
{
    poolRAII->release_connection(conRAII);
}
