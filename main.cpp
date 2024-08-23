#include<iostream>
#include"config.h"

using namespace std;

const int N=110;

int main(int argc,char*argv[] )
{
        //��Ҫ�޸ĵ����ݿ���Ϣ,��¼��,����,����    /�����Ҫ���и��ĵģ�Ҫ�ĳ����Ƿ���˵����ݿ���Ϣ
    string user="root";
    string passwd="lkt20031206";
    string database_name="webserver";
    
    //�����н���
    Config config;             //��Щ������Ϣ������װ������������ˣ�Ȼ������޸�����ĳ�Ա���޸�Ĭ��������Ϣ
    config.parse_arg(argc,argv);
    //������֮�󣬿�ʼ����webserver��

    WebServer server;

    //��ʼ��
    server.init(config.PORT, user, passwd, database_name, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model,config.Proxy);

    //��־
    server.log_write();
    //���ݿ�
    server.sql_pool();    

    //�������
    server.proxy();   

    cout<<"sql_pool init success"<<endl;
    //�̳߳�
    server.threadPool();
    cout<<"threadPool init success"<<endl;
    //����ģʽ
    server.trig_mode();
    cout<<"trig_mode init success"<<endl;
    //����
    server.event_listen();
    cout<<"event_listen init success"<<endl;
    //����
    server.event_loop();


    return 0;
}
