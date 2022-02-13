#include <stdlib.h>
#include <stdio.h> 
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <./GameHandler.h>

#ifndef _SOCKET_HANDLER
#define _SOCKET_HANDLER

#define MAX_EVENTS  1024
#define BUFLEN 128
#define SERV_PORT   8080

struct myevent_s {
    int fd;                 //cfd listenfd
    int events;             //EPOLLIN  EPLLOUT
    void *arg;              //指向自己结构体指针
    void (*call_back)(int fd, int events, void* arg, void* _ctx);
    int status;
    char buf[BUFLEN];
    int len;
    long last_active;
};

// 不知道为什么，把他们定义为类的成员函数不太好使，所以全取出来。（学艺不精）

class SocketHandler {
    public:
    unsigned short port;
    int g_efd; // epoll_create返回的句柄 
    int lfd; // 监听套接字句柄
    struct myevent_s g_events[MAX_EVENTS+1]; // +1 最后一个用于 listen fd
    struct epoll_event events[MAX_EVENTS+1]; // 上面那个是监听列表，而这个是监听到发生了的事件的列表。

    int max_id; // 之后直接全遍历吧，不要这玩意了。
    GameHandler* gameHandler;


    SocketHandler(unsigned short _port);
    int initialize();
    int setGameHandler(GameHandler* _gameHandler);
    int setPort(int port);
    int run();

    private:

    void eventset(struct myevent_s *ev, int fd, void (*call_back)(int, int, void*, void* _ctx), void* arg);
    void eventadd(int efd, int events, struct myevent_s *ev);
    void eventdel(int efd, struct myevent_s *ev);
    
    static void acceptconn(int lfd, int events, void* arg, void* _ctx); // 监听到新增连接时的回调函数
    static void recvdata(int fd, int events, void* arg, void* _ctx); // 新建立起的连接接收数据的回调函数
    static void senddata(int fd, int events, void* arg, void* _ctx);

    // 下面是负责与 GameHandler 交互的函数。
    int newUser(int fid);
    int delUser(int fid);
    int sendSocket(int fid, char* data, int len);
    int recvSocket(int fid, char* data, int len);
};

#endif