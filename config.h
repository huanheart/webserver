#ifndef CONFIG_H
#define CONFIG_H

#include"webserver.h"

using namespace std;

class Config
{
public:
    Config();
    ~Config(){};

    void parse_arg(int argc,char*argv[] );
    //�˿ں�
    int PORT;

    int Proxy=0;   //Ĭ��ѡ��nginx,��ʹ�÷������

    //��־д�뷽ʽ
    int LOGWrite;

    //�������ģʽ
    int TRIGMode;

    //listenfd����ģʽ
    int LISTENTrigmode;

    //connfd����ģʽ
    int CONNTrigmode;

    //���Źر�����
    int OPT_LINGER;

    //���ݿ����ӳ�����
    int sql_num;

    //�̳߳��ڵ��߳�����
    int thread_num;

    //�Ƿ�ر���־
    int close_log;

    //����ģ��ѡ��(��reactor���֣�
    int actor_model;


};





#endif