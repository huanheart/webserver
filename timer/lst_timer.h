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
    sockaddr_in address; //�ͻ��˵�socket��ַ
    int sockfd;
    util_timer * timer; //��ʱ��

};

//����������һ��˫��������ά���Ķ�ʱ������Ȼ��ǰ����̽��
class util_timer
{
public:
    util_timer():prev(nullptr),next(nullptr){
        cb_func=nullptr;
        user_data=nullptr;
    }
    void (*cb_func)(Client_data*); //�����Ƕ�ʱ�������˵�һ���ص�����

public:
	time_t expire; //expire����Ϊ�����ڡ�time_t��һ��ʱ���

	Client_data* user_data;
	util_timer* prev;   //�ݹ�Ƕ�ף�����������ڵ���ҽڵ�
	util_timer* next;
};


//�������ά����˫������,�����ӣ�������ɾ����Щ����
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
	void add_timer(util_timer* timer, util_timer* lst_head); //���Ŀǰ����֪���Ǹ����

    void test(); //�����õ�

	util_timer* head;
	util_timer* tail;
    Locker m_mutex;
};


class Utils
{
public:
    Utils(){}
    ~Utils(){}
    void init(int timeslot);   //ʱ϶(ʱ��������˼�� 
    //���ļ����������÷�����
    int setnonblocking(int fd);

	void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //�źŴ�����
    static void sig_handler(int sig);

    //�����źź���
    void addsig(int sig,void (handler)(int),bool restart=true);

    //��ʱ�������������¶�ʱ�Բ��ϴ���signal�ź�
    void timer_handler();

    void show_error(int connfd,const char*info);

public:
    static int * u_pipefd;  //����ǹܵ�����������ͨ�ŵ�channel����go�е�channel
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;  //һ��ʱ���źţ����ܱ�ʾ��ô���һ������źţ�����˵��ʱ���źų�ʱ�˾ͷ��������

};

void cb_func(Client_data*user_data);


#endif
