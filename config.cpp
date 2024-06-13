
#include"config.h"

Config::Config()
{
    //端口号,默认9006
    PORT = 9006;

    //日志写入方式，默认同步
    LOGWrite = 0;

    //触发组合模式,默认listenfd LT + connfd LT(水平触发模式，一直提醒的这种)
    TRIGMode = 0;

    //listenfd触发模式，默认LT
    LISTENTrigmode = 0;

    //connfd触发模式，默认LT
    CONNTrigmode = 0;

    //优雅关闭链接，默认不使用
    OPT_LINGER = 0;

    //数据库连接池数量,默认8
    sql_num = 8;

    //线程池内的线程数量,默认8
    thread_num = 8;

    //关闭日志,默认不关闭
    close_log = 0;

    //并发模型,默认是proactor
    actor_model = 0;

};


void Config::parse_arg(int argc,char * argv[] ) //这个函数主要是自己通过参数重新定义一些端口位置啊，什么的
{
    int opt;
    const char * str="p:l:m:o:s:t:c:a:";

    while( (opt=getopt(argc,argv,str) )!=-1 ) 
    {     //argc是命令行参数的数量，argv是一个指向参数字符串数组的指针，这个函数用来解析命令行参数的
        switch(opt)
        {
        case 'p':
        {
            PORT = atoi(optarg); //optarg 是一个指向当前选项参数的指针，通常用于处理带参数的选项。默认后台会更新这个东西
            break;
        }
        case 'l':
        {
            LOGWrite = atoi(optarg);
            break;
        }
        case 'm':
        {
            TRIGMode = atoi(optarg);
            break;
        }
        case 'o':
        {
            OPT_LINGER = atoi(optarg);
            break;
        }
        case 's':
        {
            sql_num = atoi(optarg);
            break;
        }
        case 't':
        {
            thread_num = atoi(optarg);
            break;
        }
        case 'c':
        {
            close_log = atoi(optarg);
            break;
        }
        case 'a':
        {
            actor_model = atoi(optarg);
            break;
        }
        default:
            break;

        }
    }

}





