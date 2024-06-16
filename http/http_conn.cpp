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
    //�����ӳ������ȡһ������
    MYSQL* mysql=nullptr;
    connectionRAII mysqlcon(&mysql,conn_pool);
    //��user���м���username,passwd���ݣ����Ƿ�������㲻���������
    if(mysql_query(mysql,"select username,passwd from user") ) //������ط�0��˵���ڲ�����
    {
        LOG_ERROR("select error:%s\n",mysql_error(mysql) );
    }

    //�ӱ��м��������Ľ������
    MYSQL_RES *result=mysql_store_result(mysql);

    //���ؽ����������
    int num_fields=mysql_num_fields(result);

    //���������ֶνṹ������
    MYSQL_FIELD * fields=mysql_fetch_fields(result); //��������ʲô���𣿶���ֲ�����Ҳû������ɶ���ó���

    while(MYSQL_ROW row=mysql_fetch_row(result) )
    {
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        users[temp1]=temp2;      //��ŵ���Ӧ���ڴ��У���ʵ����Ūһ��redis����ŵ�����
    }

}

//�����ļ�����������Ϊ������
int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_optionn=old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_optionn);
    return old_option;
}

//���ں��¼���ע����¼���ETģʽ��ѡ����EPOLLONESHOT
void addfd(int epollfd,int fd,bool one_shot,int TRIGMode)
{
    epoll_event event; //epoll�¼�
    event.data.fd=fd; //����Ҫ��ӵ��ļ�������
    // TRIGMode����ж���ʹ��ETģʽ������LTģʽ��һ�δ����Լ�һֱ����������
    if(TRIGMode==1)
        event.events =EPOLLIN | EPOLLET |EPOLLRDHUP;
    else 
        event.events=EPOLLIN |EPOLLRDHUP;
// ��ʾ�Ƿ�ʹ�� EPOLLONESHOT ѡ��������Ϊ true�����ʾ�ļ�������ֻ�ܱ�����һ�Σ�
// ��Ҫ������ӵ� epoll ʵ���У��������Ϊ false�����ʾ�ļ����������Զ�δ�����
    if(one_shot) //�����¼�����һ�λ��Ƕ��
        event.events |=EPOLLONESHOT;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}




//���ں�ʱ���ɾ��������
void removefd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0); //ɾ�����ļ�������
    close(fd);
}

void modfd(int epollfd,int fd,int ev,int TRIMode)
{
    epoll_event event;
    event.data.fd=fd;
    if(TRIMode==1)
        event.events=ev |EPOLLET |EPOLLONESHOT |EPOLLRDHUP; //������ETģʽ��ELģʽ��ѡ�񣬲������ﲻ���������б�֤�¼�ֻ����һ��
    else 
        event.events=ev | EPOLLONESHOT |EPOLLRDHUP;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event); //���������޸ģ��Ҳ�Ӱ�����������ԣ�û�н���������Щ�ĳɷ�����
}


int http_conn::m_user_count=0; //��̬��Ա������ʼ��
int http_conn::m_epollfd=-1;



//�ر����ӣ��ͻ�����-1
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


//��ʼ�����ӣ��ⲿ���ó�ʼ���׽��ֵ�ַ
void http_conn::init(int sockfd,const sockaddr_in &addr,char * root,int TRIGMode,
                     int close_log,std::string user,std::string passwd,std::string sqlname)
{
    m_sockfd=sockfd;
    m_address=addr;
    
    addfd(m_epollfd,sockfd,true,m_TRIGMode);
    m_user_count++;

 //�������������������ʱ����������վ��Ŀ¼�����http��Ӧ��ʽ������߷��ʵ��ļ���������ȫΪ�� //���ﲻ��
    doc_root=root; //���Ӧ���Ƿ������ĸ�Ŀ¼
    m_TRIGMode=TRIGMode; //�����Ǵ���ģʽ�Ĳ���
    m_close_log=close_log; 

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());
    init(); //�ڲ����ó�ʼ��
}

//��ʼ���½��յ�����
//check_stateĬ��Ϊ����������״̬
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


//��״̬����ȡ״̬�����ڷ�����һ������
//����ֵΪ�еĶ�ȡ״̬����LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line() //���ڽ�����ͬ��״̬
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
            if(m_checked_idx>1&&m_read_buf[m_checked_idx-1] =='\r')  //-1��+1��Ŀ���Ƿ�ֹ���
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

//ѭ����ȡ�ͻ����ݣ�ֱ�������ݿɶ���Է��ر�����
//������ET����ģʽ�£���Ҫһ���Խ����ݶ���
bool http_conn::read_once()
{
    std::cout<<"jin_ru_read_once"<<std::endl;
    if(m_read_idx>=READ_BUFFER_SIZE)
        return false;
    int bytes_read = 0;

    //LT��ȡ����(ˮƽ����)
    if(m_TRIGMode==0)
    {
        bytes_read=recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
        m_read_idx+=bytes_read;
        if(bytes_read<=0)
            return false;
        return true;
    }
    //ET��ȡ����
    else{
        //�����������Ҫһ���Զ�ȡ�����ݵģ���Ȼ��ѭ��etģʽ�ͷ�ѭ��etģʽ��
        
        while(true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if(bytes_read==-1)
            {
                if(errno=EAGAIN || errno==EWOULDBLOCK) //˵������û�������ˣ���Ȼ�˳�ѭ��
                    break;
                return false;  //return false˵���ڲ�������
            }
            else if(bytes_read==0) //˵��������û�ж�������
            {
                return false;
            }
            m_read_idx+=bytes_read;
           
        }   
        return true;
    }

}

//����http�����У�������󷽷���Ŀ��url�Լ��汾��
//���������������Ҫ���õ�����ĳ�Ա�ȴ��������ˣ�Ȼ�������õ�
//�����е���˼���鿴��ȸ�ʼ�
http_conn::HTTP_CODE    http_conn::parse_request_line(char* text)
{
    std::cout<<"test wei  "<<text<<std::endl;
    m_url=strpbrk(text," \t"); //���س���������ڶ����ַ��������еĵ�һ�������ַ���1���±�ĺ��������
    std::cout<<"url Ϊ  "<<m_url<<std::endl;
    if(!m_url)
    {   
        std::cout<<"cuowu 1"<<std::endl;
        return BAD_REQUEST;
    }
    *m_url++='\0';
    char * method=text;

    if(strcasecmp(method,"GET")==0) //���ڱȽϵĺ���
        m_method=GET;
    else if(strcasecmp(method,"POST")==0)
    {
        m_method=POST;
        cgi=1; //���cgi����������ģ��Ƿ����õ�post
    }
    else {
        std::cout<<"cuowu 2"<<std::endl;
        return BAD_REQUEST; ///�����û�����������������Ŀ�ƺ�ֻ������get��post
    }

    m_url+=strspn(m_url," \t" );  //���忴��ȸ
    std::cout<<"url Ϊnwenewsadasd  "<<m_url<<std::endl;


    m_version=strpbrk(m_url," \t");
    if (!m_version){
        std::cout<<"cuowu 3"<<std::endl;
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    
    m_version += strspn(m_version, " \t");
    std::cout<<m_version<<"  _ding_wei"<<std::endl;
    std::cout<<" c huxian_    "<<std::endl;
    if (strcasecmp(m_version, "HTTP/1.1") != 0) //��������
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
    if(strncasecmp(m_url,"http://",7)==0) //�ж������ַ�����ǰ7���ַ��Ƿ����
    {
        m_url+=7;
        m_url=strchr(m_url,'/'); //�������m_url��http://�����Ѿ�û���ˣ���Ȼ�����ˣ������ȽϺú�����ж�
    }


    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0]!='/' ) //˵�������˴����ˣ����ܺ���û�������ˣ�����û�н�ȡ��'/'
    {
        std::cout<<"cuowu 5"<<std::endl;
        return BAD_REQUEST;
    }
    //��urlΪ/ʱ����ʾ�жϽ���         Ϊʲô��ʱҪ
    std::cout<<"shishdsadsad    "<<m_url<<std::endl;
    if(strlen(m_url)==1)
        strcat(m_url,"judge.html");
    m_check_state= CHECK_STATE_HEADER;
     std::cout<<"shishdsadsad    "<<m_url<<std::endl;
    return NO_REQUEST;
}

//����http�����һ��ͷ����Ϣ
//��������ݻᱻѭ������
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
        m_content_length=atol(text); //��ȡ����ַ����ĳ���
    }
    else if(strncasecmp(text,"Host:",5)==0 )
    {
        text+=5;
        text+=strspn(text," \t"); //��������Ϊ��ͷ
        m_host=text;             //��ȡ������
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
    }
    
    return NO_REQUEST;
}


//�ж�http�����Ƿ���������
http_conn::HTTP_CODE http_conn::parse_content(char * text)
{
    if(m_read_idx>=(m_content_length+m_checked_idx) )
    {
        text[m_content_length]='\0';
        //post���������Ϊ������û���������
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
    //�ж����������
    std::cout<<"process_read_now"<<std::endl;
    while( (m_check_state==CHECK_STATE_CONTENT &&line_status==LINE_OK)
          || ( (line_status=parse_line() )==LINE_OK) ) //������ǰ��״̬�Ƿ�ʹ�ʱ���õ�line_status״̬һ��
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
                    std::cout<<"ʹ��bad��"<<std::endl;
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
                if(ret==GET_REQUEST) //˵����һ��������GET����
                    return do_request();
                line_status=LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR; //�������ڲ�����

        }
        return NO_REQUEST;

    }

}

http_conn::HTTP_CODE http_conn::do_request()
{
    std::cout<<"do_request now "<<std::endl;
    strcpy(m_real_file,doc_root); //���ļ��ĸ�Ŀ¼��Ȼ��ֵ��m_real_file����
    int len=strlen(doc_root);
    const char * p=strrchr(m_url,'/'); //�����ֵ���ܱ�,����'/'��һ�γ��ֵ�λ��

    //����cgi
    if(cgi==1&&(*(p+1)=='2' || *(p+1)=='3' ) )
    {
        //���ݱ�־�ж��ǵ�¼��⻹��ע����
        char flag=m_url[1];
        char *m_url_real=(char *)malloc(sizeof(char) *200);
        strcpy(m_url_real, "/");
        strcat(m_url_real,m_url+2); //��m_url+2������ַ�����ӵ�m_url_real��
        strncpy(m_real_file+len,m_url_real,FILENAME_LEN-len-1); 
        //��m_url_real���ַ�������ǰ����������Ȼ�������һ���ַ�����
        free(m_url_real); //�ͷŲ���
    
      //���û�����������ȡ����
      //user=123&passwd=123 //�ѵ�����û���������һֱ������������
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
        //�����ע�ᣬ�ȼ�����ݿ����Ƿ���������
        //û�������������
        char * sql_insert=(char*)malloc(sizeof(char)*200);
        strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
        strcat(sql_insert, "'");
        strcat(sql_insert, name);
        strcat(sql_insert, "', '");
        strcat(sql_insert, password);
        strcat(sql_insert, "')");
        //������һ����Ҫ��ƴ��һ��sql������
        if(users.find(name)==users.end() ) //˵��û���ҵ�,�û���ά����һ��map��
        {
            m_lock.lock();
            int res=mysql_query(mysql,sql_insert); //�������ӵ����ݿ������Ƿ��м�¼
            users.insert(std::pair<std::string,std::string>(name,password) );
            m_lock.unlock();
            if(!res) //˵��û�У���ôֱ������Ū����¼���棬�Ƚ���һ����ֵ
                strcpy(m_url,"/log.html");
            else 
                strcpy(m_url, "/registerError.html");
        }
        else 
            strcpy(m_url, "/registerError.html");
        //����ǵ�¼��ֱ���ж�
        //���������������û����������ڱ��п��Բ��ҵ�������1�����򷵻�0
    }
      else if (*(p + 1) == '2')
      {
        if(users.find(name)!=users.end() && users[name]==password)
            strcpy(m_url, "/welcome.html");
        else 
            strcpy(m_url, "/logError.html");
      }

    }
//�����2����3�����¼�Լ�ע��
//����ľ�������״̬�Ľ�����

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

    if(stat(m_real_file,&m_file_stat)<0)  //�������ȡ�ļ���ǰ����Ϣ
    {
        std::cout<<"meiyou ziyuan "<<std::endl;
        return NO_RESOURCE; //û����Դ
    }
    if(!(m_file_stat.st_mode)&S_IROTH) {
         std::cout<<"fang  wen jin zhi   "<<std::endl;
        return FORBIDDEN_REQUEST; //��ֹ�����״̬��
    }

    if(S_ISDIR(m_file_stat.st_mode) )
        return BAD_REQUEST;

    int fd=open(m_real_file,O_RDONLY); //ֻ���ķ�ʽ������ļ���Ȼ��������linux�£�����һ���ļ�������������ļ�����Ҫ���͵��ļ��ˣ�
    
    m_file_address=(char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0); //��ӳ�䣬���忴��ȸ(�����Ҫ�������Ƶ�ļ��������)
    close(fd);
    
    return FILE_REQUEST;

}

//������溯������������Ǹ�ӳ��
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
    if(bytes_to_send==0) //�����ֽ���==0
    {
        modfd(m_epollfd,m_sockfd,EPOLLIN,m_TRIGMode); //����epoll����
        init(); //����ǳ�ʼ���µ�����,���ܶ���Ϊ0
        return true;
    }

    while(1)
    {
        temp=writev(m_sockfd,m_iv,m_iv_count); //��ʾд��ɹ����ֽ���

        if(temp<0) //˵��д��ʧ��
        {
            if(errno==EAGAIN) //���ֿ��ܻ�û��д���Ȩ������Ϊ�Ƿ�����io������˵����ô˵���һ��ǲ�̫����
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }
        bytes_have_send += temp; //д��ĸ���+
        bytes_to_send -= temp;   //��д�ĸ���-
        if(bytes_have_send>=m_iv[0].iov_len) //˵����һ�������ݷ������ˣ�Ȼ��ʼ�Եڶ������в���
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

            if(m_linger) //�����ʲô������
            //��δ�������һ��ע��˵�� m_linger���������һ����������״̬�ı������������Ϊ true��
            //��ʾ�ڹر�����ʱ�ȴ�һ��ʱ����ȷ�����ݷ�����ϣ������������ر����ӡ�����ĳЩ����¿��������õģ�����ȷ���ͻ��˽�����������Ӧ���ݡ�
            {
                init();
                return true;
            }
            else 
                return false;

        }


    }

}

//������Ӧ��
bool http_conn::add_response(const char * format,...)
{
    if(m_write_idx>=WRITE_BUFFER_SIZE)
        return false;   
    va_list arg_list;
    va_start(arg_list,format);   //����ǿɱ��������ɶ�Ŀ�ͷ�����Ա�֤�ɱ��������һЩδ�������Ϊ
    int len=vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
    //m_write_idxΪƫ������С
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
            if(m_file_stat.st_size!=0) //���ȼ���ļ���С�Ƿ�Ϊ0
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base=m_write_buf;
                m_iv[0].iov_len=m_write_idx;
                m_iv[1].iov_base=m_file_address;
                m_iv[1].iov_len=m_file_stat.st_size;
                m_iv_count=2; //��ʾҪ�����������ݿ�
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










