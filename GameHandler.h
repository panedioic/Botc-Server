#ifndef _GAME_HANDLER
#define _GAME_HANDLER

#include <stdio.h>
#include <pthread.h>
#include <./Player.h>
#include <./Game.h>
#include <./WebServer.h>

// Info Error Fatal Log Warning

typedef unsigned long long uint64;
const int MAX_PLAYERS = 10;
const int MAX_GAMES = 3;

class Player;
class Game;
class WebServer;
class SocketHandler;

class GameHandler {
    public:
    int test;

    WebServer* webServer;
    SocketHandler* socketHandler;
    Player* playerArray[MAX_PLAYERS];
    Game* gameArray[MAX_GAMES];


    GameHandler();

    int newUser(int fid); // (deprecated)
    int delUser(int fid); // (deprecated)
    int newConnection(int fid, int type); // using RawSocket / WebSocket (1 / 2)
    int closeConnection(int fid, int type);
    // 下面这两个网络相关的函数不在 GameHandler.cpp 中实现。
    int _sendSocket(int uid, char* data, int len); // (deprecated)
    int recvSocket(int fid, char* data, int len);
    int sendSocket(int uid, char* data); // 自动获取字符串长度

    Player* getPlayerByUID(int uid);

    int newGame(int creator_uid);
    int router(int uid, char* data, int len);

    int initWebServer();
    int startWebServer();
};

void* startGame(void *threadid);
void* _startWebServer(void* webserver_ctx);

#endif