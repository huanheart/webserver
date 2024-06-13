#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h> //有fork（）这些函数
#include <signal.h>
#include <sys/types.h> //其中最常见的是pid_t、size_t、ssize_t等类型的定义，这些类型在系统编程中经常用于表示进程ID、大小和有符号大小等。
#include <sys/epoll.h>
#include <fcntl.h> //用于对已打开的文件描述符进行各种控制操作，如设置文件描述符的状态标志、非阻塞I/O等。
#include <sys/socket.h>
#include <netinet/in.h> //Internet地址族的相关结构体和符号常量,其中最常见的结构体是sockaddr_in，用于表示IPv4地址和端口号。用于网络编程中
#include <arpa/inet.h>  //声明了一些与IPv4地址转换相关的函数
#include <assert.h>     //该头文件声明了一个宏assert()，用于在程序中插入断言
#include <sys/stat.h>   //用于获取文件的状态信息，如文件的大小、访问权限等。
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h> //该头文件声明了一些与进程等待相关的函数和宏，其中最常用的函数是waitpid()，用于等待指定的子进程状态发生变化
#include <sys/uio.h>
// 该头文件声明了一些与I/O向量操作相关的函数和结构体。
// 其中最常用的结构体是iovec，它用于在一次系统调用中传输多个非连续内存区域的数据
// 例如在网络编程中使用readv()和writev()函数来进行分散读取和聚集写入。
#include <map>

#include "locker.h"
#include "sql_connection_pool.h"
#include "lst_timer.h"
#include "log.h"

class http_conn
{

public:
    static const int FILENAME_LEN = 200;       // 文件名长度
    static const int READ_BUFFER_SIZE = 2048;  // 读入的缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024; // 写入的缓冲区大小
    enum class METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE, // trace
        OPTIONS,
        CONNECT,
        PATH
    };
    enum class CHECK_STATE // 状态检查
    {
        CHECK_STATE_REQUESTLINE = 0, // 请求
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum class HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:

    http_conn() = default;
    ~http_conn() = default;

public:
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, std::string user, std::string passwd, std::string sqlname);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result(Connection_pool *conn_pool);
    int timer_flag;
    int improv;

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    int m_state; // 读为0，写为1,设置状态的
private:
    void init();
    HTTP_CODE process_read();                               // 进程读取
    bool process_write(HTTP_CODE ret);                      // 进程写入
    HTTP_CODE parse_request_line(char *text);               // 解析请求行
    HTTP_CODE parse_headers(char *text);                    // 解析头部
    HTTP_CODE parse_content(char *text);                    // 解析身体
    HTTP_CODE do_request();                                 // 做请求
    char *get_line() { return m_read_buf + m_start_line; }; // 获取行
    LINE_STATUS parse_line();                               // 解析行
    void unmap();

    // 下面应该和发送有关，上面和接收的响应有关

    bool add_response(const char *format, ...); // 增加响应
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

private:
    int m_sockfd; // 套接字的文件描述符
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    long m_read_idx; // 读取到的下标
    long m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state; // 当前检查状态包括，头部，身体等
    METHOD m_method;           // 返回一个请求类型
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    long m_content_length;
    bool m_linger;
    char *m_file_address; // 文件描述符的地址吧？
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        // 是否启用的post
    char *m_string; // 存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;

    std::map<std::string, std::string> m_users;

    int m_TRIGMode; // 触发模式
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
    



};

#endif
