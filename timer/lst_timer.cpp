#include"lst_timer.h"
#include"../http/http_conn.h"


sort_timer_lst::sort_timer_lst()
{
	head = nullptr;
	tail = nullptr;
}


sort_timer_lst::~sort_timer_lst()
{
	util_timer* temp = head;
	while (temp)
	{
		head = temp->next;
		delete temp;
		temp = head;
	}

}

void sort_timer_lst::add_timer(util_timer* timer)
{
	if (!timer)
	{
		return;
	}
	if (!head)  //说明链表为空
	{
		head = tail = timer;
		return;
	}

	if (timer->expire < head->expire) //按照到期顺序进行一个维护 
	{
		timer-> next = head; //由于这里到期时间小，固然将其放到头即可
		head->prev = timer;
		head = timer;
		return;
	}

	add_timer(timer, head); //否则进行一个内部维护
}

//内部维护排序
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
	util_timer * prev = lst_head; //先拿add来说，因为此时传入得lst_head是头节点
	util_timer* temp = prev->next; //头结点得下一个

	while (temp)
	{
		if (temp->expire < temp->expire)
		{
			prev->next = timer;
			timer->next = temp;
			temp->prev = timer;
			break;
		}
		prev = temp;
		temp = temp->next;
	}
	//如果此时得temp到达末尾了，说明此时插入得timer就是作为尾结点了，因为他是最大时间的结点
	if (!temp)
	{
		prev->next = timer;
		timer->prev = prev;
		timer->next = nullptr;
		tail = timer;
	}
}

void sort_timer_lst::adjust_timer(util_timer* timer) 
{
	if (!timer)
		return;
	
	util_timer* temp = timer->next;
	if (!temp || (timer->expire < temp->expire)) //说明这个根本不用维护了
		return;
	if (timer == head)
	{
		head = head->next;        //不过这里的话，是将其结点从链表中拆出来，
		head->prev = nullptr;
		timer->next = nullptr;
		add_timer(timer, head);      //这里还要回看，还没有看懂他要干嘛，要完成这个函数才能知道
	}
	else
	{
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		add_timer(timer, timer->next);      
		//分析为什么要这样传参(减少时间吧？因为它前面的一定会比他先超时）
		//固然到达它的时候，就不用管它前面的了，因为前面的肯定优先比它进行了adjust处理了
	}

}

void sort_timer_lst::del_timer(util_timer*timer)
{
	if(!timer)
		return ;
	if ((timer == head )&& (timer == tail) ) //说明此时就只有一个结点在链表中
	{
		delete timer;
		head = nullptr;
		tail = nullptr;
		return;
	}

	if (timer = head)
	{
		head = head->next;
		head->prev = nullptr;
		delete timer;
		return;

	}

	if (timer == tail)
	{
		tail = tail->prev;
		tail->next = nullptr;
		delete timer;
		return;
	}
	timer->prev->next = timer->next;
	timer->next->prev = timer->prev;
	delete timer;
}

void sort_timer_lst::tick()
{
	if(!head) //说明链表为空
		return ;
	time_t cur=time(nullptr);
	util_timer*temp=head;

	while(temp)
	{
		if(cur<temp->expire)
			break;
		//可能是发现当前结点的时间以及超过了当前时间，所以做超时处理了
		//将里面的用户信息放过去，还不知道要用户数据拿来干啥
		temp->cb_func(temp->user_data);
		head=temp->next;
		if(head)
			head->prev=nullptr;
		delete temp;
		temp=head;
	}

}


void Utils::init(int timeslot)
{
	m_TIMESLOT=timeslot;
}

//对socket文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
	int old_option=fcntl(fd,F_GETFL); //同个F——GETFL标志可以知道你此时调用函数是为了获取fd的状态的。
	int new_option=old_option | O_NONBLOCK; //非阻塞
	fcntl(fd,F_SETFL,new_option);
	return old_option;

}

//通过注册事件，可以告诉内核在某个文件描述符上发生特定事件时应该通知应用程序
//意味着当文件描述符上有可读数据时，内核会通知应用程
//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void Utils::addfd(int epollfd,int fd,bool one_shot,int TRIGMode)
{
	epoll_event event;
	event.data.fd=fd;
	if(TRIGMode==1)
		event.events= EPOLLIN | EPOLLET | EPOLLRDHUP;
	else 
		event.events = EPOLLIN | EPOLLRDHUP;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event); //向内核事件表注册一个事件了
	setnonblocking(fd);
}

//信号处理函数 //还不知道是干嘛的
void Utils::sig_handler(int sig)
{
	int save_errno=errno;
	int msg=sig;
		//send函数实际是将应用层发送缓冲区
	//的数据拷贝到内核缓冲区
	//并不是往网络上发送数据
	send(u_pipefd[1],(char*)&msg,1,0);
		//msg是将其int转换成char*类型了
	//0表示是阻塞的，即如果管道已经满了，那么就一直会等待
	//1表示按照字节发送，这里按照1个字节发送
	errno=save_errno;

}

//设置信号函数
//这段代码的作用是为特定信号设置信号处理函数
void Utils::addsig(int sig,void(handler)(int),bool restart)
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler=handler; //指定信号发生的时候触发函数是什么
	if(restart)
		sa.sa_flags|=SA_RESTART;; //接收道信号后是否重启
	
	// 调用sigfillset函数后，sa.sa_mask指向的信号集中的所有位都会被置为1，
	sigfillset(&sa.sa_mask);
	
	//上面这个是字段设置为包含所有信号的信号集。这样做是为了在信号处理函数执行期间阻塞所有其他信号，以防止中断信号处理函数的执行。

    //sigaction 函数来设置信号处理器
    //assert 宏用于确保 sigaction 函数调用成功
    assert(sigaction(sig, &sa, NULL) != -1);
}


//定时处理任务，重新定时以不断触发SIGNALRM信号(alarm函数到点了只会触发一次信号,固然触发了一次后需要重新触发)
void Utils::timer_handler()
{
	m_timer_lst.tick();
	alarm(m_TIMESLOT);
	//设置信号传送闹钟，将信号发送给当前的进程

}

void Utils::show_error(int connfd,const char*info)
{
	send(connfd,info,strlen(info),0);
	close(connfd);
}

int *Utils::u_pipefd=0;
int Utils::u_epollfd=0;

class Utils;


//这个函数是关闭资源的，用来取消对这个socket连接的监听
void cb_func(Client_data*user_data)
{
	//删除当前这个epoll事件监听器中当前用户的sockfd信息
	//并关闭
	epoll_ctl(Utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
	assert(user_data);
	close(user_data->sockfd);
	http_conn::m_user_count--; //这里有一个细节：就是在使用这个的时候，必须访问其头文件

}


