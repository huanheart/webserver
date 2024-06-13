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
	if (!head)  //˵������Ϊ��
	{
		head = tail = timer;
		return;
	}

	if (timer->expire < head->expire) //���յ���˳�����һ��ά�� 
	{
		timer-> next = head; //�������ﵽ��ʱ��С����Ȼ����ŵ�ͷ����
		head->prev = timer;
		head = timer;
		return;
	}

	add_timer(timer, head); //�������һ���ڲ�ά��
}

//�ڲ�ά������
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
	util_timer * prev = lst_head; //����add��˵����Ϊ��ʱ�����lst_head��ͷ�ڵ�
	util_timer* temp = prev->next; //ͷ������һ��

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
	//�����ʱ��temp����ĩβ�ˣ�˵����ʱ�����timer������Ϊβ����ˣ���Ϊ�������ʱ��Ľ��
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
	if (!temp || (timer->expire < temp->expire)) //˵�������������ά����
		return;
	if (timer == head)
	{
		head = head->next;        //��������Ļ����ǽ�����������в������
		head->prev = nullptr;
		timer->next = nullptr;
		add_timer(timer, head);      //���ﻹҪ�ؿ�����û�п�����Ҫ���Ҫ��������������֪��
	}
	else
	{
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		add_timer(timer, timer->next);      
		//����ΪʲôҪ��������(����ʱ��ɣ���Ϊ��ǰ���һ��������ȳ�ʱ��
		//��Ȼ��������ʱ�򣬾Ͳ��ù���ǰ����ˣ���Ϊǰ��Ŀ϶����ȱ���������adjust������
	}

}

void sort_timer_lst::del_timer(util_timer*timer)
{
	if(!timer)
		return ;
	if ((timer == head )&& (timer == tail) ) //˵����ʱ��ֻ��һ�������������
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
	if(!head) //˵������Ϊ��
		return ;
	time_t cur=time(nullptr);
	util_timer*temp=head;

	while(temp)
	{
		if(cur<temp->expire)
			break;
		//�����Ƿ��ֵ�ǰ����ʱ���Լ������˵�ǰʱ�䣬��������ʱ������
		//��������û���Ϣ�Ź�ȥ������֪��Ҫ�û�����������ɶ
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

//��socket�ļ����������÷�����
int Utils::setnonblocking(int fd)
{
	int old_option=fcntl(fd,F_GETFL); //ͬ��F����GETFL��־����֪�����ʱ���ú�����Ϊ�˻�ȡfd��״̬�ġ�
	int new_option=old_option | O_NONBLOCK; //������
	fcntl(fd,F_SETFL,new_option);
	return old_option;

}

//ͨ��ע���¼������Ը����ں���ĳ���ļ��������Ϸ����ض��¼�ʱӦ��֪ͨӦ�ó���
//��ζ�ŵ��ļ����������пɶ�����ʱ���ں˻�֪ͨӦ�ó�
//���ں��¼���ע����¼���ETģʽ��ѡ����EPOLLONESHOT
void Utils::addfd(int epollfd,int fd,bool one_shot,int TRIGMode)
{
	epoll_event event;
	event.data.fd=fd;
	if(TRIGMode==1)
		event.events= EPOLLIN | EPOLLET | EPOLLRDHUP;
	else 
		event.events = EPOLLIN | EPOLLRDHUP;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event); //���ں��¼���ע��һ���¼���
	setnonblocking(fd);
}

//�źŴ����� //����֪���Ǹ����
void Utils::sig_handler(int sig)
{
	int save_errno=errno;
	int msg=sig;
		//send����ʵ���ǽ�Ӧ�ò㷢�ͻ�����
	//�����ݿ������ں˻�����
	//�������������Ϸ�������
	send(u_pipefd[1],(char*)&msg,1,0);
		//msg�ǽ���intת����char*������
	//0��ʾ�������ģ�������ܵ��Ѿ����ˣ���ô��һֱ��ȴ�
	//1��ʾ�����ֽڷ��ͣ����ﰴ��1���ֽڷ���
	errno=save_errno;

}

//�����źź���
//��δ����������Ϊ�ض��ź������źŴ�����
void Utils::addsig(int sig,void(handler)(int),bool restart)
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler=handler; //ָ���źŷ�����ʱ�򴥷�������ʲô
	if(restart)
		sa.sa_flags|=SA_RESTART;; //���յ��źź��Ƿ�����
	
	// ����sigfillset������sa.sa_maskָ����źż��е�����λ���ᱻ��Ϊ1��
	sigfillset(&sa.sa_mask);
	
	//����������ֶ�����Ϊ���������źŵ��źż�����������Ϊ�����źŴ�����ִ���ڼ��������������źţ��Է�ֹ�ж��źŴ�������ִ�С�

    //sigaction �����������źŴ�����
    //assert ������ȷ�� sigaction �������óɹ�
    assert(sigaction(sig, &sa, NULL) != -1);
}


//��ʱ�����������¶�ʱ�Բ��ϴ���SIGNALRM�ź�(alarm����������ֻ�ᴥ��һ���ź�,��Ȼ������һ�κ���Ҫ���´���)
void Utils::timer_handler()
{
	m_timer_lst.tick();
	alarm(m_TIMESLOT);
	//�����źŴ������ӣ����źŷ��͸���ǰ�Ľ���

}

void Utils::show_error(int connfd,const char*info)
{
	send(connfd,info,strlen(info),0);
	close(connfd);
}

int *Utils::u_pipefd=0;
int Utils::u_epollfd=0;

class Utils;


//��������ǹر���Դ�ģ�����ȡ�������socket���ӵļ���
void cb_func(Client_data*user_data)
{
	//ɾ����ǰ���epoll�¼��������е�ǰ�û���sockfd��Ϣ
	//���ر�
	epoll_ctl(Utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
	assert(user_data);
	close(user_data->sockfd);
	http_conn::m_user_count--; //������һ��ϸ�ڣ�������ʹ�������ʱ�򣬱��������ͷ�ļ�

}


