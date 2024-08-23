#ifndef WEBSERVER_H
#define WEBSERVER_H

#include<sys/socket.h>
#include <netinet/in.h> //定义了一些网络的地址结构
#include <arpa/inet.h> //定义了一些ipv4转化函数
#include <stdio.h>
#include<unistd.h>
#include<errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include<cassert>

#include<sys/epoll.h>

#include"timer/lst_timer.h"
#include"threadpool/threadpool.h"
#include"http/http_conn.h"

const int MAX_FD=65536; //最大文件描述符
const int MAX_EVENT_NUMBER=10000; //最大事件数
const int TIMESLOT=5; //最小超时单位


class WebServer
{

public:
    //基础：
    int m_port;
    char * m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;
    int m_pipefd[2]; //管道,感觉是用来存放socket的
    int m_epollfd;  
    http_conn *users; //用户http数组，每个下标所对应的内容就是一个用户
    //数据库相关;

    Connection_pool *m_conn_pool; //数据库连接池
    std::string m_user;       //登录数据库用户名
    std::string m_password;  //登录数据库密码
    std::string m_database_name;    //使用的数据库名字
    int m_sql_num;
    

    //线程池相关
    thread_pool<http_conn> *m_pool;  //线程池
    int m_thread_num;
    //与epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode; //不知道这三个有什么区别(解析：这三个是处理不同的事情应用什么样的模块)
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //定时器相关
    Client_data * users_timer; //定时器数组，每个下标所对应的内容就是一个用户的定时器
    Utils utils;

    //反向代理与限制并发相关
    int m_decideproxy=0; 

public:
    WebServer();
    ~WebServer();

    void init(int port,std::string user,std::string password,std::string database_name,
              int log_write,int opt_linger,int trigmode,int sql_num,
              int thread_num,int close_log,int actor_model,int decideproxy);
    void threadPool();
    void sql_pool();
    void log_write();
    void proxy();
    void trig_mode();
    void event_listen();
    void event_loop();
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer*timer,int sockfd);
    bool dealclientdata(); //处理客户端数据
    bool dealwithsignal(bool&timeout,bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

};

#endif 