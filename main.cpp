#include<iostream>
#include"config.h"

using namespace std;

const int N=110;

int main(int argc,char*argv[] )
{
        //需要修改的数据库信息,登录名,密码,库名    /这个是要进行更改的，要改成我们服务端的数据库信息
    string user="root";
    string passwd="lkt20031206";
    string database_name="webserver";
    
    //命令行解析
    Config config;             //那些配置信息都被封装到这个类里面了，然后可以修改里面的成员来修改默认配置信息
    config.parse_arg(argc,argv);
    //配置完之后，开始创建webserver类

    WebServer server;

    //初始化
    server.init(config.PORT, user, passwd, database_name, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model,config.Proxy);

    //日志
    server.log_write();
    //数据库
    server.sql_pool();    

    //反向代理
    server.proxy();   

    cout<<"sql_pool init success"<<endl;
    //线程池
    server.threadPool();
    cout<<"threadPool init success"<<endl;
    //触发模式
    server.trig_mode();
    cout<<"trig_mode init success"<<endl;
    //监听
    server.event_listen();
    cout<<"event_listen init success"<<endl;
    //运行
    server.event_loop();


    return 0;
}
