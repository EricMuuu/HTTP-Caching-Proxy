CC = g++ 
CFLAGS = -Wall -pthread -std=c++17
TARGET = proxyServer
OBJS = proxy.o request.o response.o cache.o common.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

proxy.o: proxy.cpp proxy.h request.h response.h user.h cache.h common.h
	$(CC) $(CFLAGS) -c proxy.cpp

request.o: request.cpp request.h
	$(CC) $(CFLAGS) -c request.cpp

response.o: response.cpp response.h cache.h
	$(CC) $(CFLAGS) -c response.cpp

cache.o: cache.cpp cache.h response.h common.h
	$(CC) $(CFLAGS) -c cache.cpp

common.o: common.cpp common.h
	$(CC) $(CFLAGS) -c common.cpp

clean:
	rm -f $(TARGET) $(OBJS) *.o
