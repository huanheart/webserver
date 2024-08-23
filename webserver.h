#ifndef WEBSERVER_H
#define WEBSERVER_H

#include<sys/socket.h>
#include <netinet/in.h> //������һЩ����ĵ�ַ�ṹ
#include <arpa/inet.h> //������һЩipv4ת������
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

const int MAX_FD=65536; //����ļ�������
const int MAX_EVENT_NUMBER=10000; //����¼���
const int TIMESLOT=5; //��С��ʱ��λ


class WebServer
{

public:
    //������
    int m_port;
    char * m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;
    int m_pipefd[2]; //�ܵ�,�о����������socket��
    int m_epollfd;  
    http_conn *users; //�û�http���飬ÿ���±�����Ӧ�����ݾ���һ���û�
    //���ݿ����;

    Connection_pool *m_conn_pool; //���ݿ����ӳ�
    std::string m_user;       //��¼���ݿ��û���
    std::string m_password;  //��¼���ݿ�����
    std::string m_database_name;    //ʹ�õ����ݿ�����
    int m_sql_num;
    

    //�̳߳����
    thread_pool<http_conn> *m_pool;  //�̳߳�
    int m_thread_num;
    //��epoll_event���
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode; //��֪����������ʲô����(�������������Ǵ���ͬ������Ӧ��ʲô����ģ��)
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //��ʱ�����
    Client_data * users_timer; //��ʱ�����飬ÿ���±�����Ӧ�����ݾ���һ���û��Ķ�ʱ��
    Utils utils;

    //������������Ʋ������
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
    bool dealclientdata(); //����ͻ�������
    bool dealwithsignal(bool&timeout,bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

};

#endif 