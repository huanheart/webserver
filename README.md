# WebServer


linux下c++web服务器

* 实现基于**内存池的缓存数据结构**
* 使用**nginx作为反向代理**(可通过选项选择），以及可以在nginx.conf中，**限制最大流量以及并发处理数量,基于工厂模式进行管理**
* 使用 **线程池 + 非阻塞socket + epoll(ET和LT均实现) + 事件处理(Reactor和模拟Proactor均实现) 的并发模型**
* 使用状态机解析HTTP请求报文，支持解析GET和POST请求(支持http1.1请求）
* 访问服务器数据库实现web端用户注册、登录功能，可以请求服务器图片和视频文件
* 实现**同步/异步日志系统**，记录服务器运行状态
* 使用**定时器**处理未响应用户，防止资源一直被占用
* 实现**数据库连接池**，利用**单例模式**



## 运行环境要求

* 乌班图 22.04
*  mysql 8.0.37

* mysql所需环境
    ```cpp
  create database yourdb;     //yourdb根据自身喜好命名数据库名称

  // 创建user表
  USE yourdb;      //使用该数据库
  CREATE TABLE user( 
      username char(50) NULL,
      passwd char(50) NULL
  )ENGINE=InnoDB;

  // 添加数据
  INSERT INTO user(username, passwd) VALUES('name', 'passwd');
    ```
* 修改main.cpp中的数据库初始化信息
    ```cpp
    //数据库登录名，密码，以及你所创建的数据库名称
    string user="root"; //用户
    string passwd="root" //你每次使用mysql的密码
    string 
    ```
  
  


