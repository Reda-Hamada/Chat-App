CXX      = g++
CXXFLAGS = -std=c++17 -Wall -lpthread

all: server client

server: server/server.cpp common/protocol.h
	$(CXX) server/server.cpp -o server/server $(CXXFLAGS)

client: client/client.cpp common/protocol.h
	$(CXX) client/client.cpp -o client/client $(CXXFLAGS)

clean:
	rm -f server/server client/client

.PHONY: all clean
