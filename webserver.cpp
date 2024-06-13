#include"webserver.h"
#include"lst_timer.h"

WebServer::WebServer()
{
    //http_conn�����
    users = new http_conn[MAX_FD];   //users��һ�����飬Ȼ��洢����ÿ��http���ӣ���СΪ�����������
        //root�ļ���·��
    char server_path[200];
    getcwd(server_path, 200); //��ȡ��ǰ�ļ�·��
    char root[6] = "/root"; //m_root��root��һ����

    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root); //׷��

    //��ʱ��
    users_timer=new Client_data[MAX_FD];    //��ʱ������


}


WebServer::~WebServer()
{
    //�ر�һЩ�¼������ģ��ܵ�����ʱ�����Լ����ӵ�
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete []users;
    delete []users_timer;
    delete m_pool;
}

void WebServer::init(int port, std::string user, std::string passWord, std::string databaseName, int log_write, 
                     int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
   m_port = port;
   m_user = user;
   m_password = passWord;
   m_database_name = databaseName;
   m_sql_num = sql_num;
   m_thread_num = thread_num;
   m_log_write = log_write;
   m_OPT_LINGER = opt_linger;
   m_TRIGMode = trigmode;
   m_close_log = close_log;
   m_actormodel = actor_model;
}


void WebServer::trig_mode()
{
    //ע�⣺et�Ǳ�Ե����ģʽ
    //���ݴ����ֵ���ж�Ӧ����������ģʽ
    //LT+LT
    if(m_TRIGMode==0)
    {
        m_LISTENTrigmode=0;    //������ģʽ
        m_CONNTrigmode=0;   //���ӵ�ģʽ
    }
    else if (m_TRIGMode==1)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    //ET + LT
    else if (m_TRIGMode==2)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    //ET + ET
    else if (m_TRIGMode==3)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

void WebServer::log_write()
{

    if(m_close_log==0)  //Ĭ�ϲ��ر���־
    {
        //��ʼ����־
        //�����ֱ�ĺ���Ϊ��־�ļ������ƣ��ر���־�ı�ʶ���������Ĵ�С�����ڸ���������ļ��������Լ�һ���ļ����������,���������еĴ�С
        if(m_log_write==1) //��־д�뷽ʽ
            Log::get_instance()->init("./ServerLog",m_close_log,2000,800000,800);   //����Ϊ�첽����ô��ʱ���800��ʾ���첽�������еĴ�С
        else 
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0); //����Ϊͬ��
    
    }

}


void WebServer::sql_pool()
{
    //��ʼ�����ݿ����ӳ�
    m_conn_pool=Connection_pool::get_instance(); //��ȡ���������
    m_conn_pool->init("localhost",m_user,m_password,m_database_name,3306,m_sql_num,m_close_log);
                    //��Ӧurl��������ַ��Ĭ��ΪlocalHost�������ӵ����ݿ������ʲô��      //m_sql_num��ʾ�������������������е�
    //��ʼ�����ݿ��ȡ��
    // std::cout<<"m_conn_pool init success"<<std::endl;
    users->initmysql_result(m_conn_pool); //users��http_conn����

}


void WebServer::threadPool()
{
    //�̳߳�
    //����Ϊ��m_actormodel�����ϸƷ���ÿ����ݿ��http������һ����,���ӳأ��̳߳ؿ���
    m_pool =new thread_pool<http_conn>(m_actormodel,m_conn_pool,m_thread_num); 
    //��ʵ���е��ĸ�Ĭ�ϲ������������Ĭ�ϵ��ˣ�10000����������

}

void WebServer::event_listen()
{
    //��Ӧ���Ƕ�socket�ķ���˵ĳ�ʼ���׶Σ��Ȱѷ��������úü���ģʽ

    //�����̻������裺
    m_listenfd=socket(PF_INET,SOCK_STREAM,0); // //m_listenfd�Ƿ���˴������׽���
    //��һ��������ʾָ��ͨ�����򣬱���ipv4����ipv6���֣��ڶ�������֪���������ӵĿɿ����ǿɿ����Ƿǿɿ�����������ʾ�õ�Э��
    //д0����������ǰ����������ָ��Э����ʲô
    assert(m_listenfd>=0);  //����һ�����ԣ�����ʱ����


    //���Źر����ӣ�
    if(m_OPT_LINGER==0)
    {
        struct linger tmp={0,1}; 
        //��һ����Ա��ʾ�Ƿ���Ҫ����SO_LINGER ���ڶ�����Ա��ʾ�رյ�ʱ��ȴ�һ��ʱ��
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp) );
    }
    else if(m_OPT_LINGER==1)
    {
        struct linger tmp{1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp) );
    }

    int ret=0;
    struct sockaddr_in address; //��װ��ip��ַ���˿ں���Щ
    bzero(&address,sizeof(address) ); //��ǰ�ڶ���������Ϊ0

    address.sin_family=AF_INET; //ָ����ipv4
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(m_port);

    int flag=1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag) );
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret>=0);
    //���м���
    ret=listen(m_listenfd,5);
    assert(ret>=0);
    utils.init(TIMESLOT);
    

    //������epoll�����ں��¼���
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd=epoll_create(5); //�ļ��������Ĺ���ֵ�����忴��ȸ
    assert(m_epollfd!=-1);

    utils.addfd(m_epollfd,m_listenfd,false,m_LISTENTrigmode);
    http_conn::m_epollfd=m_epollfd;

    ret=socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd);
    //�������ͨ�����ڴ������ӽ���֮���ͨ�Źܵ�
    //��Ϊ�������������ļ�������֮�佨��һ��ȫ˫����ͨ��ͨ����ʹ�ø����̺��ӽ��̿����໥ͨ�š�


    assert(ret!=-1);
    utils.setnonblocking(m_pipefd[1]); //�Լ��ĺ���������Ϊ��������������ļ�������
    utils.addfd(m_epollfd, m_pipefd[0], false, 0); //�����¼������epool����ȥ���м���

    utils.addsig(SIGPIPE,SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);
    //������,�źź���������������
    Utils::u_pipefd=m_pipefd; //�����m_pipefd���洢��������Ϣ���ֱ��ǿͻ��˺ͷ���˵��ļ�������socket��
    Utils::u_epollfd=m_epollfd; //���¼�������epool���͸�������
}


void WebServer::timer(int connfd,struct sockaddr_in client_address)
{
    
    users[connfd].init(connfd,client_address,m_root,m_CONNTrigmode,
                        m_close_log,m_user,m_password,m_database_name);
    //һ��http������飬connfd���Ӧ�����ļ�������sockfd,ÿ��Ԫ��Ӧ�þ����һ���û��ɣ�

    //��ʼ�ͻ���client_data����
    //������ʱ�������ûص������ͳ�ʱʱ�䣬���û����ݣ�����ʱ����ӵ�������
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer=new util_timer(); 
    //ÿ���û�ר�Ŷ�һ����ʱ��(Ȼ���û���ָ��ʱ����Ȩ����ʱ��Ҳ��ָ���û���Ȩ��)
    timer->user_data=&users_timer[connfd]; //��ʱ��ָ���û�

    timer->cb_func=cb_func;
    time_t cur=time(nullptr);
    timer->expire=cur+3*TIMESLOT; //���ó�ʱʱ��
    users_timer[connfd].timer=timer;    //�û�ָ��ʱ��
    utils.m_timer_lst.add_timer(timer);


}

//�������ݴ��䣬�򽫶�ʱ�������ӳ�3����λ
//�����µĶ�ʱ���������ϵ�λ�ý��е���

//��Ϊ��ʱ���ڴ�������лỨʱ�䣬��֤�����ݱ����͵������Ҳ��ᱻ�ڴ���ʱ��ɾ�������ʱ������ֹ����bug
void WebServer::adjust_timer(util_timer*timer)
{
    //˵�����û����õ�һЩ��������Ȼ��ʱ��رն�ʱ����ʱ��
    time_t cur=time(nullptr);
    timer->expire=cur +3*TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);
    LOG_INFO("%s","adjust timer once");
}


void WebServer::deal_timer(util_timer *timer,int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if(timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }
    LOG_INFO("close fd %d",users_timer[sockfd].sockfd);
}


bool WebServer::dealclientdata() //����ͻ��˵����ݣ����triemode��
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength=sizeof(client_address);
    if(m_LISTENTrigmode==0) //���ж�ѭ�����ǵ��εĴ���(������ģʽ)
    {
        int connfd=accept(m_listenfd,(struct sockaddr*)&client_address,&client_addrlength);
        //���connfd�ǿͻ��˵��׽��֣����ڱ�ʾ�ͻ��˵�
        if(connfd<0)
        {
            LOG_ERROR("%s:errno is:%d","accept error",errno);
            return false;
        }
        if(http_conn::m_user_count>=MAX_FD)
        {
            utils.show_error(connfd,"Internal server busy"); //�ڲ���ͻ��˷���һ��������Ϣ
            LOG_ERROR("%s","Internal server busy"); //�����Ҳдһ����־��ȥ
            return false;
        }

        timer(connfd,client_address);   //connfd  �ǿͻ��˵��׽��֣����ڱ�ʾ�ͻ��˵�
        //Ϊʲô��ʱ�û��ֱ������ȥ�ˣ�����ɾ���ĺ�����(����ĺ���ʱ������ڲ���Ա�����������ҿ����ˣ����Ǵ������˼)

    }
    else 
    {
        while(1)     //��ʱ�ļ���������ģʽ
        {
            int connfd=accept(m_listenfd,(struct sockaddr*)&client_address,&client_addrlength);
            //���ѭ�����м�����ǰ�Ľӿ��Ƿ�����Ϣ������
            if(connfd<0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if(http_conn::m_user_count>=MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
            }
            timer(connfd,client_address);
        }
        return false;
    }
    return true;
}


bool WebServer::dealwithsignal(bool & timeout,bool & stop_server) 
//����ͻ��˷��͹��������ݣ��鿴�Ƿ�ʱʲô��.Ӧ�û��������ط�������ȷ�����ݣ��Ͼ����ﷵ��true�ˣ��ж��������ֵ
//Ȼ�����Ǹ������е�������������Եģ��ڴ���400��������ȷʵ���к������õ�ǰ���������
{
    int ret=0;
    int sig;
    char signals[1024];
    ret=recv(m_pipefd[0],signals,sizeof(signals),0); 
    //���ص�ret�������>0��ʾ�ɹ����յ������ݸ���
    if(ret==-1)
        return false;
    else if(ret==0)
        return false;
    else 
    {
        for(int i=0;i<ret;++i) //��ÿ������������
        {
            switch(signals[i])
            {
                case SIGALRM:
                {
                    timeout=true;
                    break;
                }
                case SIGTERM:
                {
                    stop_server=true;
                    break;
                }
            }
        }
    }
    return true;
}


void WebServer::dealwithread(int sockfd)
{
    util_timer *timer=users_timer[sockfd].timer;
    std::cout<<"chu_xian_"<<std::endl;
    //reactorģ�ͣ�
    if(m_actormodel==1) //model�кܶ��ֵģ��ֱ������жϲ�ͬ���¼������ַ�ʽ
    {
        if(timer)
            adjust_timer(timer);

        //�����⵽���¼��������¼����뵽���������
        m_pool->append(users+sockfd,0);
        
        while(true)
        {
            if(users[sockfd].timer_flag==1) //�Ǳ�ʾ��Ҫɾ��
            {
                deal_timer(timer,sockfd);
                users[sockfd].timer_flag=0;
            }
            users[sockfd].improv=0;
            break;

        }

    }
    else 
    {
        //proactor�첽������ģ�Ͳ���
        if(users[sockfd].read_once() )   //true��ʾ�ɹ���false��ʾʧ��
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            //��������ǰ��ַ�Ŀͻ�������
            m_pool->append_p(users+sockfd);
            if(timer)
                adjust_timer(timer);
            
        }
        else 
            deal_timer(timer,sockfd);

    }

}

void WebServer::dealwithwrite(int sockfd)
{
    util_timer * timer=users_timer[sockfd].timer;
    //reactor
    if(m_actormodel==1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        m_pool->append(users + sockfd, 1); //������д
        //����users+sockfd�е㲻��(��ʵ���ǵ�ǰ�û������users��һ��http�û�����)
        while(true)
        {
            if(users[sockfd].improv==1) //improv���庬�岻�Ǻܶ�
            {
                if(users[sockfd].timer_flag==1) //˵�������ˣ���ʱʲô��,��Ҫɾ����
                {
                    deal_timer(timer,sockfd);
                    users[sockfd].timer_flag=0;
                }
                users[sockfd].improv=0;
                break;
            }
        }
    }
    else 
    {
       //proactor
       if(users[sockfd].write())
       {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr)); //ͨ��sock���Ի�ȡ����ַ
            //inet_ntoa
            if(timer)
                adjust_timer(timer);
            else 
                deal_timer(timer,sockfd); 
                //˵��������,��Ϊ������û�������ʱ����,���Ƿ���sockfd���ɴ��ڣ�Ϊʲô���ڣ��������������ˣ���Ȼ�����sockfd�ļ���������
                //������û��ʱ�������Ի���Ҫ�������ɾ����ʱ����������Ϊ���ڲ���������ɾ���˼�ʱ������ɾ���˶�Ӧ��ʱ����sockfd
       }

    }
}

void WebServer::event_loop()
{

    bool timeout=false;
    bool stop_server=false;

    while(!stop_server)
    {
        int number=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1); //�������Ϊ -1�����ʾ�����ȴ�ֱ�����¼�����,���Ὣ���ݷŵ���Ӧ
        //�û��ṩ��������
        if(number<0&&errno!=EINTR)     //˵��������
        {
            LOG_ERROR("%s","epoll failure");
            break;
        }

        for(int i=0;i<number;++i)
        {
            int sockfd=events[i].data.fd;
            if(sockfd==m_listenfd)  //m_listenfd�Ƿ���˴������׽���
            {     //�����sockfd�����ǿͻ��˵��׽��֣���ȵ�ʱ���ʾ���µĿͻ���������ˣ���Ȼ����Ϊ֮�����µ��׽���

                bool flag=dealclientdata();  //����ͻ������ݣ������;�д�����ôֱ�ӷ��أ�Ȼ�������һ��epoll�¼�
                if(flag==false)
                    continue;
            }
            else if(events[i].events&(EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //�������˹ر����ӣ��Ƴ���Ӧ�Ķ�ʱ��
                util_timer *timer=users_timer[sockfd].timer;
                deal_timer(timer,sockfd);
            } 
            //�����ź�
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            { //m_pipefd�����ʾ�ܵ��������ݿɶ�����֮ǰ���õ�alarm�������Ϲ�ϵ��
                //�����Ǵ���û��ͷ���˵��ļ����������Ǵ洢һЩ�ź����ݣ�alarmÿ����һ�ξͻ�������ܵ��з��͸��źŵ�

                bool flag=dealwithsignal(timeout,stop_server);
                if(flag==false)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //����ͻ������Ͻ��յ�������
            else if(events[i].events&EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if(events[i].events&EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }

        }
        if(timeout)
        {
            utils.timer_handler();
            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }

}






















