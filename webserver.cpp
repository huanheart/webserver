#include"webserver.h"
#include"lst_timer.h"

WebServer::WebServer()
{
    //http_conn类对象
    users = new http_conn[MAX_FD];   //users是一个数组，然后存储的是每个http连接（大小为最大连接数）
        //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200); //获取当前文件路径
    char root[6] = "/root"; //m_root和root不一样的

    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root); //追加

    //定时器
    users_timer=new Client_data[MAX_FD];    //定时器数组


}


WebServer::~WebServer()
{
    //关闭一些事件监听的，管道，定时器，以及池子等
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
    //注意：et是边缘触发模式
    //根据传入的值来判断应该属于哪种模式
    //LT+LT
    if(m_TRIGMode==0)
    {
        m_LISTENTrigmode=0;    //监听的模式
        m_CONNTrigmode=0;   //连接的模式
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

    if(m_close_log==0)  //默认不关闭日志
    {
        //初始化日志
        //参数分别的含义为日志文件的名称，关闭日志的标识，缓冲区的大小（用于辅助输出的文件名），以及一个文件的最大行数,与阻塞队列的大小
        if(m_log_write==1) //日志写入方式
            Log::get_instance()->init("./ServerLog",m_close_log,2000,800000,800);   //设置为异步，那么此时这个800表示其异步阻塞队列的大小
        else 
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0); //设置为同步
    
    }

}


void WebServer::sql_pool()
{
    //初始化数据库连接池
    m_conn_pool=Connection_pool::get_instance(); //获取单例类池子
    m_conn_pool->init("localhost",m_user,m_password,m_database_name,3306,m_sql_num,m_close_log);
                    //对应url（主机地址，默认为localHost），连接的数据库的名字什么的      //m_sql_num表示最大连接数，放入池子中的
    //初始化数据库读取表
    // std::cout<<"m_conn_pool init success"<<std::endl;
    users->initmysql_result(m_conn_pool); //users是http_conn类型

}


void WebServer::threadPool()
{
    //线程池
    //参数为：m_actormodel这个得细品，得看数据库和http搭配那一块了,连接池，线程池开的
    m_pool =new thread_pool<http_conn>(m_actormodel,m_conn_pool,m_thread_num); 
    //其实还有第四个默认参数，这里就用默认的了，10000个请求数量

}

void WebServer::event_listen()
{
    //这应该是对socket的服务端的初始化阶段，先把服务器设置好监听模式

    //网络编程基本步骤：
    m_listenfd=socket(PF_INET,SOCK_STREAM,0); // //m_listenfd是服务端创建的套接字
    //第一个参数表示指明通信领域，比如ipv4或者ipv6这种，第二个参数知名面向连接的可靠性是可靠还是非可靠，第三个表示用的协议
    //写0表明，根据前两个参数来指明协议是什么
    assert(m_listenfd>=0);  //进行一个断言，运行时触发


    //优雅关闭连接：
    if(m_OPT_LINGER==0)
    {
        struct linger tmp={0,1}; 
        //第一个成员表示是否需要禁用SO_LINGER ，第二个成员表示关闭的时候等待一定时间
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp) );
    }
    else if(m_OPT_LINGER==1)
    {
        struct linger tmp{1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp) );
    }

    int ret=0;
    struct sockaddr_in address; //封装了ip地址，端口号这些
    bzero(&address,sizeof(address) ); //将前第二个参数置为0

    address.sin_family=AF_INET; //指的是ipv4
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(m_port);

    int flag=1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag) );
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret>=0);
    //进行监听
    ret=listen(m_listenfd,5);
    assert(ret>=0);
    utils.init(TIMESLOT);
    

    //现在是epoll创建内核事件表：
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd=epoll_create(5); //文件描述符的估计值，具体看语雀
    assert(m_epollfd!=-1);

    utils.addfd(m_epollfd,m_listenfd,false,m_LISTENTrigmode);
    http_conn::m_epollfd=m_epollfd;

    ret=socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd);
    //这个函数通常用于创建父子进程之间的通信管道
    //因为它可以在两个文件描述符之间建立一个全双工的通信通道，使得父进程和子进程可以相互通信。


    assert(ret!=-1);
    utils.setnonblocking(m_pipefd[1]); //自己的函数，设置为非阻塞，将这个文件描述符
    utils.addfd(m_epollfd, m_pipefd[0], false, 0); //增加事件到这个epool里面去进行监听

    utils.addsig(SIGPIPE,SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);
    //工具类,信号和描述符基础操作
    Utils::u_pipefd=m_pipefd; //将这个m_pipefd（存储了两个信息，分别是客户端和服务端的文件描述符socket）
    Utils::u_epollfd=m_epollfd; //将事件监听器epool传送给工具类
}


void WebServer::timer(int connfd,struct sockaddr_in client_address)
{
    
    users[connfd].init(connfd,client_address,m_root,m_CONNTrigmode,
                        m_close_log,m_user,m_password,m_database_name);
    //一种http类的数组，connfd这个应该是文件描述符sockfd,每个元素应该就针对一个用户吧？

    //初始客户端client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer=new util_timer(); 
    //每个用户专门对一个定时器(然后用户有指向定时器的权力定时器也有指向用户的权力)
    timer->user_data=&users_timer[connfd]; //定时器指向用户

    timer->cb_func=cb_func;
    time_t cur=time(nullptr);
    timer->expire=cur+3*TIMESLOT; //设置超时时间
    users_timer[connfd].timer=timer;    //用户指向定时器
    utils.m_timer_lst.add_timer(timer);


}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整

//因为定时器在传输过程中会花时间，保证了数据被传送到，而且不会被在传输时侯删除这个定时器，防止出现bug
void WebServer::adjust_timer(util_timer*timer)
{
    //说明该用户有用到一些操作，固然延时其关闭定时器的时间
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


bool WebServer::dealclientdata() //处理客户端的数据，这个triemode是
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength=sizeof(client_address);
    if(m_LISTENTrigmode==0) //是判断循环还是单次的处理，(非阻塞模式)
    {
        int connfd=accept(m_listenfd,(struct sockaddr*)&client_address,&client_addrlength);
        //这个connfd是客户端的套接字，用于表示客户端的
        if(connfd<0)
        {
            LOG_ERROR("%s:errno is:%d","accept error",errno);
            return false;
        }
        if(http_conn::m_user_count>=MAX_FD)
        {
            utils.show_error(connfd,"Internal server busy"); //内部向客户端发送一个错误信息
            LOG_ERROR("%s","Internal server busy"); //服务端也写一个日志进去
            return false;
        }

        timer(connfd,client_address);   //connfd  是客户端的套接字，用于表示客户端的
        //为什么此时用户又被添加上去了？不是删除的函数吗？(这里的函数时自身的内部成员函数，而且我看错了，这是处理的意思)

    }
    else 
    {
        while(1)     //此时的监听是阻塞模式
        {
            int connfd=accept(m_listenfd,(struct sockaddr*)&client_address,&client_addrlength);
            //多次循环进行监听当前的接口是否用信息传过来
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
//处理客户端发送过来的数据？查看是否超时什么的.应该会有其他地方处理正确的数据，毕竟这里返回true了，判断这个返回值
//然后在那个函数中调用这个函数。对的，在代码400多行那里确实会有函数调用当前这个函数的
{
    int ret=0;
    int sig;
    char signals[1024];
    ret=recv(m_pipefd[0],signals,sizeof(signals),0); 
    //返回的ret数据如果>0表示成功接收到的数据个数
    if(ret==-1)
        return false;
    else if(ret==0)
        return false;
    else 
    {
        for(int i=0;i<ret;++i) //对每个数据做处理
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
    //reactor模型：
    if(m_actormodel==1) //model有很多种的，分别用来判断不同的事件用哪种方式
    {
        if(timer)
            adjust_timer(timer);

        //如果监测到读事件，将该事件放入到请求队列中
        m_pool->append(users+sockfd,0);
        
        while(true)
        {
            if(users[sockfd].timer_flag==1) //是表示需要删除
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
        //proactor异步网络编程模型采用
        if(users[sockfd].read_once() )   //true表示成功，false表示失败
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            //表明处理当前地址的客户端数据
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
        m_pool->append(users + sockfd, 1); //这里是写
        //这里users+sockfd有点不懂(其实就是当前用户，这个users是一个http用户数组)
        while(true)
        {
            if(users[sockfd].improv==1) //improv具体含义不是很懂
            {
                if(users[sockfd].timer_flag==1) //说明出错了，超时什么的,需要删除了
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
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr)); //通过sock可以获取到地址
            //inet_ntoa
            if(timer)
                adjust_timer(timer);
            else 
                deal_timer(timer,sockfd); 
                //说明出错了,因为都根本没有这个计时器了,但是发现sockfd依旧存在（为什么存在，都传进来参数了，固然有这个sockfd文件描述符）
                //但是又没计时器。所以还是要调用这个删除计时器函数，因为他内部不仅仅是删除了计时器，还删除了对应计时器的sockfd
       }

    }
}

void WebServer::event_loop()
{

    bool timeout=false;
    bool stop_server=false;

    while(!stop_server)
    {
        int number=epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1); //如果设置为 -1，则表示阻塞等待直到有事件发生,他会将内容放到对应
        //用户提供的数组中
        if(number<0&&errno!=EINTR)     //说明出错了
        {
            LOG_ERROR("%s","epoll failure");
            break;
        }

        for(int i=0;i<number;++i)
        {
            int sockfd=events[i].data.fd;
            if(sockfd==m_listenfd)  //m_listenfd是服务端创建的套接字
            {     //这里的sockfd并不是客户端的套接字，相等的时候表示有新的客户请求过来了，固然我们为之分配新的套接字

                bool flag=dealclientdata();  //处理客户端数据，如果中途有错误，那么直接返回，然后监听下一个epoll事件
                if(flag==false)
                    continue;
            }
            else if(events[i].events&(EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                util_timer *timer=users_timer[sockfd].timer;
                deal_timer(timer,sockfd);
            } 
            //处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            { //m_pipefd这个表示管道中有数据可读，跟之前调用的alarm函数扯上关系了
                //并不是存放用户和服务端的文件描述符，是存储一些信号内容，alarm每调用一次就会往这个管道中发送个信号的

                bool flag=dealwithsignal(timeout,stop_server);
                if(flag==false)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户连接上接收到的数据
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






















