#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include<stdio.h>
#include<list>
#include<error.h>
#include<string>
#include<string.h>
#include<iostream>
#include<mysql/mysql.h>

#include"log.h"
#include"locker.h"

class Connection_pool
{

private:
    Connection_pool()=default; //由于是单例类，固然这么样
    ~Connection_pool();

    int m_max_conn; //最大连接数
    int m_cur_conn=0;  //当前已经使用的连接数
    int m_free_conn=0; //当前空闲的连接数
    Locker lock; 

    std::list<MYSQL*> conn_list; //连接池
    Sem reserve;


public:
    std::string m_url;  //主机地址
    std::string m_port; //数据库端口号
    std::string m_user; //登录数据库用户名
    std::string m_passwd; //密码
    std::string m_databasename; //使用数据库名
    int m_close_log; //日志开关


public:

    MYSQL*get_connection(); //获取数据库连接
    bool release_connection(MYSQL*conn); //释放连接
    int get_free_conn(); //获取连接
    void destroy_pool();      //销毁所有连接

    static Connection_pool *get_instance();
	void init(std::string url, std::string user, 
			std::string passwd, std::string data_base_name,
			int port, int max_conn, int close_log);


};



class connectionRAII{

public:
	connectionRAII(MYSQL **con, Connection_pool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;    //这里要看一下具体是干嘛的(在初始化的时候，将RAII这个类的conRAII与poolRAII都进行了一个初始化)
	Connection_pool *poolRAII;
};

#endif
