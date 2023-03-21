CXX ?= g++
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: main.cpp ./timer.cpp ./webserver.cpp ./sql_connection_pool.cpp ./log.cpp ./http_connection.cpp ./config.cpp
	$(CXX) -o server $^ $(CXXFLAGS) -lpthread -lmysqlclient

claen:
	rm -r server