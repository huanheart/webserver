# WebServer


## linux��c++web���������ܸ���

* ʵ�ֻ���**�ڴ�صĻ������ݽṹ**
* ʹ��**nginx��Ϊ�������**(��ͨ��ѡ��ѡ�񣩣��Լ�������nginx.conf�У�**������������Լ�������������,���ڹ���ģʽ���й���**
* ʹ�� **�̳߳� + ������socket + epoll(ET��LT��ʵ��) + �¼�����(Reactor��ģ��Proactor��ʵ��) �Ĳ���ģ��**
* ʹ��״̬������HTTP�����ģ�֧�ֽ���GET��POST����(֧��http1.1����
* ���ʷ��������ݿ�ʵ��web���û�ע�ᡢ��¼���ܣ��������������ͼƬ����Ƶ�ļ�
* ʵ��**ͬ��/�첽��־ϵͳ**����¼����������״̬
* ʹ��**��ʱ��**����δ��Ӧ�û�����ֹ��Դһֱ��ռ
* ʵ��**���ݿ����ӳ�**������**����ģʽ**
* ������ģʽ��ÿ���û�������Դ�����ͷţ��ٴ����󽫻����´�����ʱ���Լ�����socketfd

## ��Ŀ��չ

- [ ] ��Э�̿⵼�����Ŀ��ͨ��Э�̿��ڲ�epoll�滻��webserver���е�epoll�����Ч��
- [ ] ���ӳ����ӻ��ƣ����������������Ѿ������ӵĺ����л�
- [X] �����˶�ʱ�����ֵ�bug


## ���л���Ҫ��

* �ڰ�ͼ 22.04
*  mysql 8.0.37
* nginx������ο�:https://help.fanruan.com/finereport10.0/doc-view-2644.html
* ��makefile , g++��ع���

* mysql������ĵĵط�
    ```cpp
  create database yourdb;     //yourdb��������ϲ���������ݿ�����

  // ����user��
  USE yourdb;      //ʹ�ø����ݿ�
  CREATE TABLE user( 
      username char(50) NULL,
      passwd char(50) NULL
  )ENGINE=InnoDB;

  // �������
  INSERT INTO user(username, passwd) VALUES('name', 'passwd');
    ```
* �޸�main.cpp�е����ݿ��ʼ����Ϣ
    ```cpp
    //���ݿ��¼�������룬�Լ��������������ݿ�����
    string user="root"; //�û�
    string passwd="root" //��ÿ��ʹ��mysql������
    string databasename="yourdb"; //�㴴�������ݿ������
    ```

* nginx :����/usr/nginx/confĿ¼�£�ʹ��vim��nginx����Ϊ����
  * ��location��Ŀ¼ȫ������Ϊ����
    ```cpp
            location  / {
            proxy_pass http://192.168.15.128:9006;          #��Ϊwebserver��ip��ַ�Լ��˿ں�
            proxy_http_version 1.1;                      #����http�汾�������������ô���������ΪnginxĬ�Ϸ���http1.0���󣬵��Ǹ�webserver��Ӧ����1.0����
            limit_conn addr 1;                           #����������󲢷���
            #limit_rate  50;   #���ƶ�Ӧ��Ӧ�����ʣ�������50��Ĭ�ϱ�ʾ50bit�����ƣ�׷���ٶȿ�Ҳ����ֱ��Ū��1m
            limit_conn_log_level warn;
            proxy_set_header X-Forwarded-By "Nginx";   #��http��ʶͷ���һ��nginx��ʶ������Ҳ�����(���ݸ�webserver�߼����Կ�����)

        }
    ```
    *  ��keepalive_timeout 65; �¶��һ�仰
      ```cpp
      limit_conn_zone $binary_remote_addr zone=addr:5m;  #��������Ϊ5m�Ŀռ䣬�乲���ڴ������Ϊaddr����ʱ��ʵ�Ϳ��Դ���10�����������
      ```
* ������Ŀ:
  * cd��webserverĿ¼��
  * make //���б���
  * ./webserver    //������Ŀ
  * ��ѡ��nginx��ʱ�򣬸��������ĵ���Ӧ��ȥ��/usr/nginx/sbin�Ƚ���Ӧnginx��������(./nginx),ҲҪ�鿴��ǰnginx�����Ķ˿��Ƿ�ռ��
  * �����������ip:�˿�.   **����192.168.12.23:80**   �����ʵ���Ӧ������(������webserver��-nѡ��Ϊnginx����ôip��ַ����Ϊnginx��ip��ַ����������webserver��ip��ַ+�˿�,**ע��Ĭ����Ҫ����nginx��ip��ַ��������ʲ���**)


## ���Ի�����
```cpp
./server [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model] [-n Proxy]
```

* -p �Զ���˿ں�
  * Ĭ��9006
* -l ѡ����־д�뷽ʽ��Ĭ��ͬ��д��
  * 0,ͬ��д��
  * 1.�첽д��
* -m listenfd��connfd��ģʽ��ϣ�Ĭ��ʹ��LT + LT      (LTˮƽ����,ET��Ե����)
  * 0,ʹ��LT+LT
  * 1,ʹ��LT+ET
  * 2,ʹ��ET+LT
  * 3,ʹ��ET+ET
* -O ���Źر����ӣ�Ĭ�ϲ�ʹ��
  * 0,��ʹ��
  * 1,ʹ��
* -t �߳�����
  * Ĭ��Ϊ8
* -c �Ƿ�����־ϵͳ��Ĭ�ϴ�
  * 0,����־
  * 1,�ر���־
* -a ѡ��Ӧ��ģ�ͣ�Ĭ��proactor
  * 0 Proactorģ��
  * 1 Reactorģ��
* -n ѡ���Ƿ������������,Ĭ������nginx,������Ӱ�������м������
  * 0 ����nginx
  * 1 ��ʹ���κ��м������       
  
  
## ѹ������
���ÿ���˵ķ��������ܲ�ͬ����ѹ������qps����������ͬ��������ͬһ����������nginx�ĺ͵�ǰwebserver��qps���жԱ�


```cpp
#Linux��ʹ��wrk����
#��װ
sudo apt-get install wrk
#ʹ��ʾ��
#-t12��ʾʹ��12���߳�,-c400��ʾ����400����������,-d30��ʾ����ʱ��Ϊ30s
wrk -t12 -c400 -d30s -H "Connection: keep-alive" http://192.168.15.128:80
```

