
CXXFLAGS = -w

#变量定义
objects =main.o log.o sql_connection_pool.o lst_timer.o http_conn.o config.o webserver.o

#最终目标文件
edit :$(objects)
	g++ -o   webserver $(objects) -lmysqlclient  -g
main.o : config.h
log.o  : log.h block_queue.h
sql_connection_pool.o : sql_connection_pool.h locker.h log.h
lst_timer.o :lst_timer.h log.h http_conn.h
http_conn.o :http_conn.h locker.h sql_connection_pool.h lst_timer.h log.h
config.o : config.h webserver.h  threadpool.h  http_conn.h
webserver.o : webserver.h lst_timer.h


.PHONY : clean
clean:
	rm  webserver  $(objects)

