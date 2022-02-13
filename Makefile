#botc_server: botc.cpp SocketHandler.cpp GameHandler.cpp Player.cpp Game.cpp WebServer.cpp SocketHandler.h GameHandler.h Player.h Game.h WebServer.h
#	@g++ botc.cpp SocketHandler.cpp GameHandler.cpp Player.cpp Game.cpp ./mongoose/mongoose.c WebServer.cpp -I ./ -o ./botcs -lpthread -std=c++11

CC = g++
FLAG = -g 
INCLUDE = -I ./
LIBDIR =
LIB = -std=c++11 -lpthread 
BIN = 
TARGET = botcs
SRCS = botc.cpp SocketHandler.cpp GameHandler.cpp Player.cpp Game.cpp ./mongoose/mongoose.c WebServer.cpp

$(TARGET):$(SRCS:.cpp=.o)
	$(CC) $(FLAG) $(LIBDIR) -o $@ $^ $(LIB)

%.o:%.cpp
	$(CC) $(FLAG) $(INCLUDE) -c -o $@ $<

clean:
	-rm -f *.o *.d
