#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <time.h>

#include"../log/log.h"
#include "../lock/locker.h"

class util_timer;

struct  Client_data
{
public:
    sockaddr_in address; //客户端的socket地址
    int sockfd;
    util_timer * timer; //定时器

};

//由于这里是一个双向链表来维护的定时器，固然有前驱后继结点
class util_timer
{
public:
    util_timer():prev(nullptr),next(nullptr){
        cb_func=nullptr;
        user_data=nullptr;
    }
    void (*cb_func)(Client_data*); //可能是定时器到期了的一个回调函数

public:
	time_t expire; //expire翻译为：到期。time_t是一个时间戳

	Client_data* user_data;
	util_timer* prev;   //递归嵌套，类似树的左节点和右节点
	util_timer* next;
};


//这个就是维护的双向链表,有增加，调整，删除这些功能
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer * timer);
    void adjust_timer(util_timer* timer);
    void del_timer(util_timer* timer);
    void tick();

private:
	void add_timer(util_timer* timer, util_timer* lst_head); //这个目前还不知道是干嘛的

    void test(); //调试用的

	util_timer* head;
	util_timer* tail;
    Locker m_mutex;
};


class Utils
{
public:
    Utils(){}
    ~Utils(){}
    void init(int timeslot);   //时隙(时间间隔的意思） 
    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

	void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig,void (handler)(int),bool restart=true);

    //定时器处理任务，重新定时以不断触发signal信号
    void timer_handler();

    void show_error(int connfd,const char*info);

public:
    static int * u_pipefd;  //这个是管道，可能用于通信的channel，即go中的channel
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;  //一个时间信号，可能表示多久触发一次这个信号？还是说此时的信号超时了就发送这个？

};

void cb_func(Client_data*user_data);


#endif
