#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

Locker m_lock;
std::map<std::string, std::string> users;


void http_conn::initmysql_result(Connection_pool *conn_pool)
{
    //从连接池里面获取一个连接
    MYSQL* mysql=nullptr;
    connectionRAII mysqlcon(&mysql,conn_pool);
    //在user表中检索username,passwd数据，看是否可以满足不报错的条件
    if(mysql_query(mysql,"select username,passwd from user") ) //如果返回非0，说明内部错误
    {
        LOG_ERROR("select error:%s\n",mysql_error(mysql) );
    }

    //从表中检索完整的结果集：
    MYSQL_RES *result=mysql_store_result(mysql);

    //返回结果集的列数
    int num_fields=mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD * fields=mysql_fetch_fields(result); //这两个有什么用吗？定义局部变量也没看到有啥利用场景

    while(MYSQL_ROW row=mysql_fetch_row(result) )
    {
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        users[temp1]=temp2;      //存放到对应的内存中，其实可以弄一个redis，存放到里面
    }

}

//对其文件描述符设置为非阻塞
int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_optionn=old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_optionn);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd,int fd,bool one_shot,int TRIGMode)
{
    epoll_event event; //epoll事件
    event.data.fd=fd; //表明要添加的文件描述符
    // TRIGMode这个判断你使用ET模式还是用LT模式，一次触发以及一直触发的区别
    if(TRIGMode==1)
        event.events =EPOLLIN | EPOLLET |EPOLLRDHUP;
    else 
        event.events=EPOLLIN |EPOLLRDHUP;
// 表示是否使用 EPOLLONESHOT 选项。如果设置为 true，则表示文件描述符只能被触发一次，
// 需要重新添加到 epoll 实例中；如果设置为 false，则表示文件描述符可以多次触发。
    if(one_shot) //决定事件触发一次还是多次
        event.events |=EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}




//从内核时间表删除描述符
void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0); //删除此文件描述符
    close(fd);
}

void modfd(int epollfd,int fd,int ev,int TRIMode)
{
    epoll_event event;
    event.data.fd=fd;
    if(TRIMode==1)
        event.events=ev |EPOLLET |EPOLLONESHOT |EPOLLRDHUP; //这里是ET模式和EL模式的选择，不过这里不管哪样都有保证事件只触发一次
    else 
        event.events=ev | EPOLLONESHOT |EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event); //且这里是修改，且不影响其阻塞特性，没有将阻塞的那些改成非阻塞
}


int http_conn::m_user_count=0; //静态成员变量初始化
int http_conn::m_epollfd=-1;



//关闭连接，客户总量-1
void http_conn::close_conn( bool real_close)
{

    if(real_close&&(m_sockfd!=-1) )
    {
        printf("close %d\n",m_sockfd);
        removefd(m_epollfd,m_sockfd);
        m_sockfd=-1;
        m_user_count--;
    }


}


//初始化连接，外部调用初始化套接字地址
void http_conn::init(int sockfd,const sockaddr_in &addr,char * root,int TRIGMode,
                     int close_log,std::string user,std::string passwd,std::string sqlname)
{
    m_sockfd=sockfd;
    m_address=addr;
    
    addfd(m_epollfd,sockfd,true,m_TRIGMode);
    m_user_count++;

 //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空 //这里不懂
    doc_root=root; //这个应该是服务器的根目录
    m_TRIGMode=TRIGMode; //可能是触发模式的参数
    m_close_log=close_log; 

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());
    init(); //内部重置初始化
}

//初始化新接收的连接
//check_state默认为分析请求行状态
void http_conn::init()
{
    mysql=nullptr; 
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE::CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = METHOD::GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);

}


//从状态机获取状态，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line() //用于解析不同的状态
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if(temp=='\r')
        {
            if( (m_checked_idx+1) ==m_read_idx) 
                return LINE_OPEN;
            else if(m_read_buf[m_checked_idx+1]=='\n')
            {
                m_read_buf[m_checked_idx++]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;
            }
        }else if(temp=='\n'){
            if(m_checked_idx>1&&m_read_buf[m_checked_idx-1] =='\r')  //-1和+1的目的是防止溢出
            {
                m_read_buf[m_checked_idx-1]='\0';
                m_read_buf[m_checked_idx++]='\0';
                return LINE_OK;        
            }
            return  LINE_BAD;
        }
    }
    std::cout<<"hksahdkahskd  "<<std::endl;
    return LINE_OPEN;

}

//循环读取客户数据，直到无数据可读或对方关闭连接
//非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
    std::cout<<"jin_ru_read_once"<<std::endl;
    if(m_read_idx>=READ_BUFFER_SIZE)
        return false;
    int bytes_read = 0;

    //LT读取数据(水平触发)
    if(m_TRIGMode==0)
    {
        bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        m_read_idx+=bytes_read;
        if(bytes_read<=0)
            return false;
        return true;
    }
    //ET读取数据
    else{
        //由于这个是需要一次性读取完数据的，固然有循环et模式和非循环et模式，
        
        while(true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if(bytes_read==-1)
            {
                if(errno=EAGAIN || errno==EWOULDBLOCK) //说明后面没有数据了，固然退出循环
                    break;
                return false;  //return false说明内部出错了
            }
            else if(bytes_read==0) //说明根本就没有读的内容
            {
                return false;
            }
            m_read_idx+=bytes_read;
           
        }   
        return true;
    }

}

//解析http请求行，获得请求方法，目标url以及版本号
//这里解析的内容主要放置到了类的成员先储存起来了，然后后面会用到
//请求行的意思详情看语雀笔记
http_conn::HTTP_CODE    http_conn::parse_request_line(char* text)
{
    std::cout<<"test wei  "<<text<<std::endl;
    m_url=strpbrk(text," \t"); //返回出现在这个第二个字符串集合中的第一个属于字符串1的下标的后面的内容
    std::cout<<"url 为  "<<m_url<<std::endl;
    if(!m_url)
    {   
        std::cout<<"cuowu 1"<<std::endl;
        return BAD_REQUEST;
    }
    *m_url++='\0';
    char * method=text;

    if(strcasecmp(method,"GET")==0) //用于比较的函数
        m_method=GET;
    else if(strcasecmp(method,"POST")==0)
    {
        m_method=POST;
        cgi=1; //这个cgi是用来干嘛的？是否启用的post
    }
    else {
        std::cout<<"cuowu 2"<<std::endl;
        return BAD_REQUEST; ///如果都没有这两个请求，这个项目似乎只处理了get和post
    }

    m_url+=strspn(m_url," \t" );  //具体看语雀
    std::cout<<"url 为nwenewsadasd  "<<m_url<<std::endl;


    m_version=strpbrk(m_url," \t");
    if (!m_version){
        std::cout<<"cuowu 3"<<std::endl;
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    
    m_version += strspn(m_version, " \t");
    std::cout<<m_version<<"  _ding_wei"<<std::endl;
    std::cout<<" c huxian_    "<<std::endl;
    if (strcasecmp(m_version, "HTTP/1.1") != 0) //如果不相等
    {
        std::cout<<"cuowu 4"<<std::endl;
        return HTTP_CODE::BAD_REQUEST;
    }
    // std::string temp="HTTP/1.1";
    // for(int i=0;i<temp.size();i++){
    //     if(temp[i]!=m_version[i]){
    //         std::cout<<"cuowu 4"<<std::endl;
    //         return BAD_REQUEST;
    //     }
    // }
    if(strncasecmp(m_url,"http://",7)==0) //判断两个字符串的前7个字符是否相等
    {
        m_url+=7;
        m_url=strchr(m_url,'/'); //可能这个m_url的http://这里已经没用了，固然往后了，这样比较好后面的判断
    }


    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0]!='/' ) //说明出现了错误了，可能后面没有内容了，所以没有截取到'/'
    {
        std::cout<<"cuowu 5"<<std::endl;
        return BAD_REQUEST;
    }
    //当url为/时，显示判断界面         为什么此时要
    std::cout<<"shishdsadsad    "<<m_url<<std::endl;
    if(strlen(m_url)==1)
        strcat(m_url,"judge.html");
    m_check_state= CHECK_STATE_HEADER;
     std::cout<<"shishdsadsad    "<<m_url<<std::endl;
    return NO_REQUEST;
}

//解析http请求的一个头部信息
//这里的内容会被循环调用
http_conn::HTTP_CODE  http_conn::parse_headers(char *text)
{
    if(text[0]=='\0')
    {
        if(m_content_length!=0)
        {
            m_check_state=CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text,"Connection:",11)==0) 
    {
        text+=11;
        text+=strspn(text," \t");
        if(strcasecmp(text,"keep-alive")==0)
        {
            m_linger=true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text+=15;
        text+=strspn(text," \t");
        m_content_length=atol(text); //获取这个字符串的长度
    }
    else if(strncasecmp(text,"Host:",5)==0 )
    {
        text+=5;
        text+=strspn(text," \t"); //重置这里为开头
        m_host=text;             //获取其内容
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    
    return NO_REQUEST;
}


//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char * text)
{
    if(m_read_idx>=(m_content_length+m_checked_idx) )
    {
        text[m_content_length]='\0';
        //post请求中最后为输入的用户名和密码
        m_string=text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE  http_conn::process_read()
{
    LINE_STATUS line_status=LINE_OK;
    HTTP_CODE ret=NO_REQUEST;
    char * text=0;
    //判断请求的内容
    std::cout<<"process_read_now"<<std::endl;
    while( (m_check_state==CHECK_STATE_CONTENT &&line_status==LINE_OK)
          || ( (line_status=parse_line() )==LINE_OK) ) //分析当前行状态是否和此时设置的line_status状态一致
    {
        std::cout<<"process_read_now_now"<<std::endl;
        text=get_line();

        m_start_line=m_checked_idx;
        LOG_INFO("%s",text);
        std::cout<<text<<"   bushi laodi"<<std::endl;
        std::cout<<m_check_state<<"  shu chu ba"<<std::endl;
        switch (m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret=parse_request_line(text);
                if(ret==BAD_REQUEST){
                    std::cout<<"使得bad了"<<std::endl;
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret=parse_headers(text);
                if(ret==BAD_REQUEST)
                    return BAD_REQUEST;
                else if(ret==GET_REQUEST)
                    return do_request();
                break;
            }
            case CHECK_STATE_CONTENT :
            {
                ret=parse_content(text);
                if(ret==GET_REQUEST) //说明是一个完整的GET请求
                    return do_request();
                line_status=LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR; //服务器内部错误

        }
        return NO_REQUEST;

    }

}

http_conn::HTTP_CODE http_conn::do_request()
{
    std::cout<<"do_request now "<<std::endl;
    strcpy(m_real_file,doc_root); //将文件的根目录，然后赋值到m_real_file这里
    int len=strlen(doc_root);
    const char * p=strrchr(m_url,'/'); //里面的值不能变,查找'/'第一次出现的位置

    //处理cgi
    if(cgi==1&&(*(p+1)=='2' || *(p+1)=='3' ) )
    {
        //根据标志判断是登录检测还是注册检测
        char flag=m_url[1];
        char *m_url_real=(char *)malloc(sizeof(char) *200);
        strcpy(m_url_real, "/");
        strcat(m_url_real,m_url+2); //将m_url+2后面的字符串添加到m_url_real中
        strncpy(m_real_file+len,m_url_real,FILENAME_LEN-len-1); 
        //将m_url_real的字符串复制前第三个参数然后给到第一个字符串中
        free(m_url_real); //释放操作
    
      //将用户名和密码提取出来
      //user=123&passwd=123 //难道这个用户名和密码一直都是这玩意吗？
    char name[100],password[100];
    
    {
        int i;
        for(i=5;m_string[i]!='&';++i)
            name[i-5]=m_string[i];
        name[i-5]='\0';
        int j=0;
        for(i=i+10;m_string[i]!='\0';++i,++j)
            password[j]=m_string[i];
        password[j]='\0';

    }
    if(*(p+1)=='3')
    {
        //如果是注册，先检测数据库中是否有重名的
        //没有重名才允许加
        char * sql_insert=(char*)malloc(sizeof(char)*200);
        strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
        strcat(sql_insert, "'");
        strcat(sql_insert, name);
        strcat(sql_insert, "', '");
        strcat(sql_insert, password);
        strcat(sql_insert, "')");
        //上面这一个主要是拼接一个sql语句而已
        if(users.find(name)==users.end() ) //说明没有找到,用户表维护在一个map中
        {
            m_lock.lock();
            int res=mysql_query(mysql,sql_insert); //访问连接的数据库里面是否有记录
            users.insert(std::pair<std::string,std::string>(name,password) );
            m_lock.unlock();
            if(!res) //说明没有，那么直接让他弄到登录界面，先进行一个赋值
                strcpy(m_url,"/log.html");
            else 
                strcpy(m_url, "/registerError.html");
        }
        else 
            strcpy(m_url, "/registerError.html");
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
    }
      else if (*(p + 1) == '2')
      {
        if(users.find(name)!=users.end() && users[name]==password)
            strcpy(m_url, "/welcome.html");
        else 
            strcpy(m_url, "/logError.html");
      }

    }
//上面的2或者3代表登录以及注册
//下面的就是其他状态的界面了

    if(*(p+1)=='0')
    {
        char *m_url_real=(char*)malloc(sizeof(char)*200 );
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file+len,m_url_real,strlen(m_url_real) );
        free(m_url_real);

    }
    else if(*(p+1)=='1')
    {
        char * m_url_real=(char*)malloc(sizeof(char)*200 );
        strcpy(m_url_real,"/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if(*(p+1)=='5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p+1)=='6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if(*(p+1)=='7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);

    }
    else 
        strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);

    if(stat(m_real_file,&m_file_stat)<0)  //从里面获取文件当前的信息
    {
        std::cout<<"meiyou ziyuan "<<std::endl;
        return NO_RESOURCE; //没有资源
    }
    if(!(m_file_stat.st_mode)&S_IROTH) {
         std::cout<<"fang  wen jin zhi   "<<std::endl;
        return FORBIDDEN_REQUEST; //禁止请求的状态码
    }

    if(S_ISDIR(m_file_stat.st_mode) )
        return BAD_REQUEST;

    int fd=open(m_real_file,O_RDONLY); //只读的方式打开这个文件，然后由于在linux下，返回一个文件描述符（这个文件就是要发送的文件了）
    
    m_file_address=(char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0); //做映射，具体看语雀(这个和要传输的视频文件相关联的)
    close(fd);
    
    return FILE_REQUEST;

}

//解除上面函数最后所做的那个映射
void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address=0;
    }

}

bool http_conn::write()
{
    int temp=0;
    std::cout<<"jin ru write"<<std::endl;
    if(bytes_to_send==0) //发送字节数==0
    {
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode); //重置epoll函数
        init(); //这个是初始化新的连接,将很多置为0
        return true;
    }

    while(1)
    {
        temp=writev(m_sockfd,m_iv,m_iv_count); //表示写入成功的字节数

        if(temp<0) //说明写入失败
        {
            if(errno==EAGAIN) //这种可能还没有写入的权力，因为是非阻塞io（但是说是这么说，我还是不太懂）
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }
        bytes_have_send += temp; //写入的个数+
        bytes_to_send -= temp;   //待写的个数-
        if(bytes_have_send>=m_iv[0].iov_len) //说明第一个的数据发送完了，然后开始对第二个进行操作
        {
            m_iv[0].iov_len=0;
            m_iv[1].iov_base=m_file_address+(bytes_have_send-m_write_idx);
            m_iv[1].iov_len=bytes_to_send;
        }
        else 
        {
            m_iv[0].iov_base=m_write_buf+bytes_have_send;
            m_iv[0].iov_len=m_iv[0].iov_len-bytes_have_send;

        }

        if(bytes_have_send<=0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if(m_linger) //这个是什么？不懂
            //这段代码中有一处注释说到 m_linger，这可能是一个控制连接状态的变量，如果设置为 true，
            //表示在关闭连接时等待一段时间以确保数据发送完毕，而不是立即关闭连接。这在某些情况下可能是有用的，比如确保客户端接收完整的响应数据。
            {
                init();
                return true;
            }
            else 
                return false;

        }


    }

}

//增加响应：
bool http_conn::add_response(const char * format,...)
{
    if(m_write_idx>=WRITE_BUFFER_SIZE)
        return false;   
    va_list arg_list;
    va_start(arg_list,format);   //这个是可变参数的那啥的开头，可以保证可变参数产生一些未定义的行为
    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
    //m_write_idx为偏移量大小
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }

    m_write_idx+=len;
    va_end(arg_list);
    LOG_INFO("request:%s ",m_write_buf);
    return true;
}

bool http_conn::add_status_line(int status,const char * title)
{
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}

bool http_conn::add_headers(int content_len)
{
    return add_content_length(content_len)&&add_linger()&&add_blank_line();
}

bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n",content_len);
}


bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n","text/html");
}

bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::add_content(const char * content)
{
    return add_response("%s",content);
}

bool http_conn::process_write(HTTP_CODE ret)
{
    std::cout<<"process_write"<<std::endl;
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500,error_500_title); //
            add_headers(strlen(error_500_form) );
            if(!add_content(error_500_form) )
                return false;
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200,ok_200_title);
            if(m_file_stat.st_size!=0) //首先检查文件大小是否为0
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base=m_write_buf;
                m_iv[0].iov_len=m_write_idx;
                m_iv[1].iov_base=m_file_address;
                m_iv[1].iov_len=m_file_stat.st_size;
                m_iv_count=2; //表示要发送两个数据块
                bytes_to_send=m_write_idx+m_file_stat.st_size;
                return true;
            }
            else 
            {
                const char *ok_string="<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string) )
                    return false;
            }
        }
        default:
            return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;

}

void http_conn::process()
{
    HTTP_CODE read_ret=process_read();
    std::cout<<"process_read"<<std::endl;
    if(read_ret==NO_REQUEST)
    {
        std::cout<<"HTTP_CODE::NO_REQUEST"<<std::endl;
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode);
        return ;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}










