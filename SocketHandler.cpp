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

#define MAX_EVENTS  1024
#define BUFLEN 128
#define SERV_PORT   8080

#include <./SocketHandler.h>
#include <./GameHandler.h>

void SocketHandler::eventset(struct myevent_s *ev, int fd, void (*call_back)(int, int, void*, void*), void *arg){
    ev->fd = fd;                     //cfd listenfd
    ev->call_back = call_back;
    ev->events = 0;                  //EPOLLIN  EPLLOUT
    ev->arg = arg;                   //指向自己结构体指针
    ev->status = 0;                  // 0 - ADD | 1 - MOD | 默认为0（用于 eventadd()。）(???)
    // memset(ev->buf, 0, sizeof(ev->buf));
    // ev->len = 0;
    ev->last_active = time(NULL);

    return;
}

void SocketHandler::eventadd(int efd, int events, struct myevent_s * ev){
    // 相当于对 epoll_ctl() 的封装（？）
    struct epoll_event epv = {0, {0}};
    int op;
    epv.data.ptr = ev;
    epv.events = ev->events = events;

    if(ev->status == 1) {
        op = EPOLL_CTL_MOD;
    } else {
        op = EPOLL_CTL_ADD;
        ev->status = 1;
    }

    if (epoll_ctl(efd, op, ev->fd, &epv) < 0){
        printf("[Error] [Server] Event add failed [fd=%d], events[%d]\n", ev->fd, events);
    } else {
        //printf("[Info ] [Server] Event add OK [fd=%d], op=%d, events[%0X]\n", ev->fd, op, events);
    }
    return;
}

// 这个函数就先copy了，之后再仔细研究。不过看名字应该就知道用途了。
void SocketHandler::eventdel(int efd, struct myevent_s *ev) {
    struct epoll_event epv = {0, {0}};
    if (ev->status != 1){
        return;
    }
    epv.data.ptr = ev;
    ev->status = 0;
    epoll_ctl(efd, EPOLL_CTL_DEL, ev->fd, &epv);
    return;
}

void SocketHandler::acceptconn(int lfd, int events, void *arg, void* _ctx) {
    struct sockaddr_in c_in; // 用于储存新连入的地址信息
    SocketHandler* ctx = (SocketHandler*)_ctx;
    socklen_t len = sizeof(c_in);
    int cfd, i;

    if((cfd = accept(lfd, (struct sockaddr*)&c_in, &len)) == -1){
        if(errno != EAGAIN && errno != EINTR) {
            // 暂时不处理出错信息。
        }
        printf("[Error] [Server] %s: accept, %s\n", __func__, strerror(errno));
        return;
    }
    // 这里原来的代码的 do-while 应该是用来当 goto 用的，先写好看看能不能用吧。
    do {
        // 如果 status==0，说明他还没有 event_add 过。 
        for(i = 0; i < MAX_EVENTS; i++){
            if(ctx->g_events[i].status == 0){
                break;
            }
        }
        // 遍历整个数组也没找到，说明连接满了
        if(i == MAX_EVENTS){
            printf("[Error] [Server] %s: max connect limit[%d]\n", __func__, MAX_EVENTS);
            break;
        }

        // 同构造函数中的那样，将刚刚建立起的连接设为非阻塞。
        int flag = 0;
        flag = fcntl(cfd, F_SETFL, O_NONBLOCK);
        if (flag < 0){
            printf("[Error] [Server] %s: fcntl nonblocking failed, %s\n", __func__, strerror(errno));
            break;
        }

        // 最后将描述符添加进列表中。
        // [&](*call_back)(int fd, int events, void *arg){recvdata(fd, events, arg);}
        ctx->eventset(&ctx->g_events[i], cfd, recvdata, &ctx->g_events[i]);
        ctx->eventadd(ctx->g_efd, EPOLLIN, &ctx->g_events[i]);

        //ctx->newUser(cfd); // todo
        ctx->gameHandler->newConnection(cfd, 1);
    } while (0);
    
    printf("[Info ] [Server] new connect [%s:%d][time:%ld], pos[%d]\n", inet_ntoa(c_in.sin_addr), ntohs(c_in.sin_port), ctx->g_events[i].last_active, i);
    return; 
}

// 新建立起的连接接收数据的回调函数
void SocketHandler::recvdata(int fd, int events, void* arg, void* _ctx){
    struct myevent_s* ev = (struct myevent_s*) arg;
    SocketHandler* ctx = (SocketHandler*)_ctx;
    int len;
    len = recv(fd, ev->buf, sizeof(ev->buf), 0);
    //ctx->eventdel(ctx->g_efd, ev); // （这里先删掉，如果不是断开连接的事件再在底下新建事件。
    
    if (len > 0) {
        // 有数据输入
        ev->len = len;
        ev->buf[len] = '\0';
        printf("[Info ] [Client] Recv[fd=%d]: %s", fd, ev->buf); // 这里后面就不加回车符了，毕竟在发送的时候都会有一个回车的。
        // 转换为发送事件（添加发送事件）
        //ctx->eventset(ev, fd, senddata, ev);
        //ctx->eventadd(ctx->g_efd, EPOLLIN | EPOLLOUT, ev);
        // char tmp[256];
        // sprintf(tmp, "[Client - %d] %s", (int)(ev - ctx->g_events), ev->buf);
        // int len2 = strlen(tmp);
        // for(int i = 0; i < MAX_EVENTS; i++){
        //     if(ctx->g_events[i].status == 0){
        //         break;
        //     }
        //     send(ctx->g_events[i].fd, tmp, len2, 0);
        // }
        ctx->recvSocket(fd, ev->buf, len);
    } else if (len == 0) {
        // 连接断开
        close(ev->fd);
        ctx->eventdel(ctx->g_efd, ev); // ?
        // ev-g_events 地址相减得到偏移元素位置
        //ctx->delUser(ev->fd);
        ctx->gameHandler->closeConnection(ev->fd, 1);
        printf("[Info ] [Client] [fd=%d] pos[%d], closed\n", fd, (int)(ev - ctx->g_events));
    } else {
        // Error.
        close(ev->fd);
        ctx->eventdel(ctx->g_efd, ev); // ?
        printf("[Error] [Client] recv[fd=%d] error[%d]:%s\n", fd, errno, strerror(errno));
    }
    return;
}

void SocketHandler::senddata(int fd, int events, void *arg, void* _ctx){
    struct myevent_s* ev = (struct myevent_s*) arg;
    SocketHandler* ctx = (SocketHandler*)_ctx;
    int len;
    len = send(fd, ev->buf, ev->len, 0);

    ctx->eventdel(ctx->g_efd, ev);

    if(len > 0) {
        printf("[Info ] [Client] Send[fd=%d]: %s", fd, ev->buf);
        // ? ctx->eventset(ev, fd, recvdata, ev);
        // ? ctx->eventadd(ctx->g_efd, EPOLLIN, ev);
    } else {
        // Error.
        close(ev->fd);
        printf("[Error] [Client] send[fd=%d] error %s\n", fd, strerror(errno));
    }
    return;
}

SocketHandler::SocketHandler(unsigned short _port){
    port = _port;
    g_efd = epoll_create(MAX_EVENTS+1);

    if(g_efd <= 0){
        printf("[FATAL] [Server] Create efd in %s err %s\n", __func__, strerror(errno));
    }

    lfd = socket(AF_INET, SOCK_STREAM, 0); // 创建监听套接字句柄
    fcntl(lfd, F_SETFL, O_NONBLOCK); // 获取 lfd 非阻塞描述符

    // 添加监听新连接的事件
    eventset(&g_events[MAX_EVENTS], lfd, acceptconn, &g_events[MAX_EVENTS]);
    eventadd(g_efd, EPOLLIN, &g_events[MAX_EVENTS]);

    return;
}

int SocketHandler::run(){
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);
    int ret;
    ret = bind(lfd, (struct sockaddr*) &sin, sizeof(sin));
    listen(lfd, 20);

    if(ret < 0){
        printf("[FATAL] [Server] Bind Failure: %s.\n", strerror(errno));
        return -1;
    }

    printf("[Info ] [Server] Server running: port[%d]\n", port);
    int checkpos = 0, i; // （源代码写的是这样的，不过我没看懂emm）

    while(1){
        // 超时验证，每次测试100个链接，不测试listenfd 当客户端60秒内没有和服务器通信，则关闭此客户端链接
        long now = time(NULL);
        for(i = 0;i < 100;i++,checkpos++){
            if(checkpos == MAX_EVENTS) {
                checkpos = 0;
            }

            if(g_events[checkpos].status != 1){
                continue;
            }

            long duration = now - g_events[checkpos].last_active;
            if(duration >= 900) {
                close(g_events[checkpos].fd);
                printf("[Info ] [Client] [fd=%d] timeout\n", g_events[checkpos].fd);
                eventdel(g_efd, &g_events[checkpos]);
            }
        }

        // 等待事件发生
        int nfd = epoll_wait(g_efd, events, MAX_EVENTS+1, 1000);
        if (nfd < 0) {
            printf("[FATAL] [Server] epoll_wait error with %d [%s], exit\n", nfd, strerror(errno));
            break;
        }
        for(i = 0;i < nfd;i++){
            struct myevent_s* ev = (struct myevent_s*) events[i].data.ptr;
            // 这里就直接copy了，应该没什么理解难度吧。。。
            if ((events[i].events & EPOLLIN) && (ev->events & EPOLLIN)) {
                //printf("[Debug] 11, %d\n", events[i].events);
                ev->call_back(ev->fd, events[i].events, ev->arg, this);
                //printf("[Debug] 12\n");
            }
            if ((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT)) {
                //printf("[Debug] 21, %d\n", events[i].events);
                ev->call_back(ev->fd, events[i].events, ev->arg, this);
                //printf("[Debug] 22\n");
            }
        }
    }

    return 0;
}

int SocketHandler::setPort(int port){
    return 0;
}

int SocketHandler::setGameHandler(GameHandler* _gameHandler){
    gameHandler = _gameHandler;
    return 0;
}

int SocketHandler::newUser(int fid){
    //printf("deg5 %d\n", fid);
    printf("333%p\n", this);
    gameHandler->newUser(fid);
    return 0;
}

int SocketHandler::delUser(int fid){
    gameHandler->delUser(fid);
    return 0;
}

int SocketHandler::sendSocket(int fid, char* data, int len){
    send(fid, data, len, 0);

    return 0;
}

// 这个函数。。。。
// int SocketHandler::sendSocket(int fid, char* data, int len)
int GameHandler::_sendSocket(int uid, char* data, int len){
    if(playerArray[uid] == NULL){
        return -1;
    }
    int fid = playerArray[uid]->fid;
    send(fid, data, len, 0);
    return 0;
}

int SocketHandler::recvSocket(int fid, char* data, int len){
    int i;
    for(i = 0;i < MAX_PLAYERS; i++){
        if(gameHandler->playerArray[i]->fid == fid){
            break;
        }
    }
    if(i == MAX_PLAYERS){
        return -1;
    }
    gameHandler->recvSocket(i, data, len);
    return 0;
}

