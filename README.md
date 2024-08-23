# WebServer


## linux下c++web服务器功能概述

* 实现基于**内存池的缓存数据结构**
* 使用**nginx作为反向代理**(可通过选项选择），以及可以在nginx.conf中，**限制最大流量以及并发处理数量,基于工厂模式进行管理**
* 使用 **线程池 + 非阻塞socket + epoll(ET和LT均实现) + 事件处理(Reactor和模拟Proactor均实现) 的并发模型**
* 使用状态机解析HTTP请求报文，支持解析GET和POST请求(支持http1.1请求）
* 访问服务器数据库实现web端用户注册、登录功能，可以请求服务器图片和视频文件
* 实现**同步/异步日志系统**，记录服务器运行状态
* 使用**定时器**处理未响应用户，防止资源一直被占
* 实现**数据库连接池**，利用**单例模式**
* 短连接模式，每当用户请求资源将会释放，再次请求将会重新创建定时器以及创建socketfd

## 项目扩展

- [ ] 将协程库导入该项目，通过协程库内部epoll替换掉webserver类中的epoll，提高效率
- [ ] 增加长连接机制，并且做到长连接已经短连接的合适切换
- [ ] 基于内存池的缓存链表更改为基于内存池的缓存跳表
- [X] 利用GDB定位到项目异常访问导致段错误的位置为定时器，而后更改了定时器的bug


## 运行环境要求

* 乌班图 22.04
*  mysql 8.0.37
* nginx，具体参考:https://help.fanruan.com/finereport10.0/doc-view-2644.html
* 有makefile , g++相关工具

* mysql所需更改的地方
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
    string databasename="yourdb"; //你创建的数据库的名称
    ```

* nginx :进入/usr/nginx/conf目录下，使用vim将nginx更改为如下
  * 将location下目录全部更改为如下
    ```cpp
            location  / {
            proxy_pass http://192.168.15.128:9006;          #改为webserver的ip地址以及端口号
            proxy_http_version 1.1;                      #设置http版本，如果不设置那么将会出错（因为nginx默认发送http1.0请求，但是该webserver响应不了1.0请求
            limit_conn addr 1;                           #用于设置最大并发数
            #limit_rate  50;   #限制对应响应的速率，可以用50（默认表示50bit来限制）追求速度快也可以直接弄成1m
            limit_conn_log_level warn;
            proxy_set_header X-Forwarded-By "Nginx";   #将http标识头多加一个nginx标识，不加也会出错(根据该webserver逻辑可以看出来)

        }
    ```
    *  在keepalive_timeout 65; 下多加一句话
      ```cpp
      limit_conn_zone $binary_remote_addr zone=addr:5m;  #将其设置为5m的空间，其共享内存的名字为addr，此时其实就可以处理10几万的数量了
      ```
* 启动项目:
  * cd在webserver目录下
  * make //进行编译
  * ./webserver    //启动项目
  * 当选择nginx的时候，根据上述文档，应该去到/usr/nginx/sbin先将对应nginx进行启动(./nginx),也要查看当前nginx监听的端口是否被占用
  * 在浏览器输入ip:端口.   **举例192.168.12.23:80**   即访问到对应服务器(若开启webserver的-n选项为nginx，那么ip地址输入为nginx的ip地址，否则输入webserver的ip地址+端口,**注意默认需要输入nginx的ip地址，否则访问不到**)


## 个性化运行
```cpp
./server [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model] [-n Proxy]
```

* -p 自定义端口号
  * 默认9006
* -l 选择日志写入方式，默认同步写入
  * 0,同步写入
  * 1.异步写入
* -m listenfd和connfd的模式组合，默认使用LT + LT      (LT水平触发,ET边缘触发)
  * 0,使用LT+LT
  * 1,使用LT+ET
  * 2,使用ET+LT
  * 3,使用ET+ET
* -O 优雅关闭连接，默认不使用
  * 0,不使用
  * 1,使用
* -t 线程数量
  * 默认为8
* -c 是否开启日志系统，默认打开
  * 0,打开日志
  * 1,关闭日志
* -a 选择反应堆模型，默认proactor
  * 0 Proactor模型
  * 1 Reactor模型
* -n 选择是否启动反向代理,默认启动nginx,后续会加阿帕奇等中间服务器
  * 0 启动nginx
  * 1 不使用任何中间服务器       
  
  
## 压力测试
针对每个人的服务器性能不同，测压出来的qps将会有所不同，可以拿同一个服务器上nginx的和当前webserver的qps进行对比


```cpp
#Linux上使用wrk工具
#安装
sudo apt-get install wrk
#使用示例
#-t12表示使用12个线程,-c400表示建立400个并发连接,-d30表示测试时间为30s
wrk -t12 -c400 -d30s -H "Connection: keep-alive" http://192.168.15.128:80
```

