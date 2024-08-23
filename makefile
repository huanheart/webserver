# 编译器和编译选项
CXX = g++
CXXFLAGS = -g -O0 -w

# 源文件和目标文件
objects = main.o log/log.o cglmysql/sql_connection_pool.o timer/lst_timer.o http/http_conn.o config.o webserver.o

# 最终目标文件
edit: $(objects)
	$(CXX) $(CXXFLAGS) -o webserver $(objects) -lmysqlclient

# 各个源文件的编译规则
main.o: config.h
	$(CXX) $(CXXFLAGS) -c main.cpp

log.o: log/log.h log/block_queue.h
	$(CXX) $(CXXFLAGS) -c log/log.cpp

sql_connection_pool.o: cglmysql/sql_connection_pool.h lock/locker.h log/log.h
	$(CXX) $(CXXFLAGS) -c cglmysql/sql_connection_pool.cpp

lst_timer.o: timer/lst_timer.h log/log.h http/http_conn.h
	$(CXX) $(CXXFLAGS) -c timer/lst_timer.cpp

http_conn.o: http/http_conn.h lock/locker.h cglmysql/sql_connection_pool.h timer/lst_timer.h log/log.h my_stl/my_stl.hpp memorypool/memorypool.hpp proxy/proxy.hpp
	$(CXX) $(CXXFLAGS) -c http/http_conn.cpp

config.o: config.h webserver.h threadpool/threadpool.h http/http_conn.h
	$(CXX) $(CXXFLAGS) -c config.cpp

webserver.o: webserver.h timer/lst_timer.h
	$(CXX) $(CXXFLAGS) -c webserver.cpp

# 清理目标文件
.PHONY: clean
clean:
	rm -f webserver $(objects)

