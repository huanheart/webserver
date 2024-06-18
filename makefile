
CXXFLAGS = -w

#鍙橀噺瀹氫箟
objects =main.o log/log.o  cglmysql/sql_connection_pool.o  timer/lst_timer.o   http/http_conn.o   config.o   webserver.o

#鏈€缁堢洰鏍囨枃浠�
edit :$(objects)
	g++  -g -o   webserver $(objects) -lmysqlclient 
main.o : config.h
log.o  : log/log.h log/block_queue.h
sql_connection_pool.o : cglmysql/sql_connection_pool.h lock/locker.h log/log.h
lst_timer.o :timer/lst_timer.h log/log.h  http/http_conn.h
http_conn.o :http/http_conn.h  lock/locker.h  cglmysql/sql_connection_pool.h  timer/lst_timer.h  log/log.h  my_stl/my_stl.hpp memorypool/memorypool.hpp
config.o : config.h webserver.h  threadpool/threadpool.h  http/http_conn.h
webserver.o : webserver.h timer/lst_timer.h


.PHONY : clean
clean:
	rm  webserver  $(objects)

