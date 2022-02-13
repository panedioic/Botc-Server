#include <stdio.h>
#include <./SocketHandler.h>
#include <./GameHandler.h>


int main(){
    printf("Hello world!\n");
    
    GameHandler gameHandler = GameHandler();
    SocketHandler socketHandler = SocketHandler(7772);
    socketHandler.setGameHandler(&gameHandler);
    socketHandler.setPort(7770);

    gameHandler.initWebServer();
    gameHandler.startWebServer();

    socketHandler.run();

    return 0;
}


// SocketHandler: 主要负责封装网络相关函数。负责收发远程数据，取得数据发送者的uid和具体内容转交给GameHandler。
// GameHandler: 主要负责管理游戏的全局信息，将接收到的玩家发送到数据转换成游戏中的格式储存。
