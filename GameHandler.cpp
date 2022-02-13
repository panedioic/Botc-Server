

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <./GameHandler.h>
#include <./SocketHandler.h>
#include <./WebServer.h>
#include <./Game.h>
#include <./Player.h>

GameHandler::GameHandler(){
    test = 0;
    // Then set the uid as 0 and 1 as a special uid, 
    // which means that the player does not exist and the player has exited, respectively.
    for(int i = 0;i < MAX_PLAYERS;i++){
        playerArray[i] = NULL;
    }
    for(int i = 0;i < MAX_GAMES;i++){
        gameArray[i] = NULL;
    }
}

// deprecated
int GameHandler::newUser(int fid){
    int i = 0;
    for(i = 0;i < MAX_PLAYERS;i++){
        if(playerArray[i] == NULL){
            break;
        }
    }
    if(i == MAX_PLAYERS){
        return -1;
    }
    
    //printf("[d] %d", this->test);
    Player *tmp = new Player(this);
    //printf("?????????\n");
    //playerArray[i]->gameHandler = this;
    playerArray[i] = tmp;
    playerArray[i]->fid = fid;
    playerArray[i]->uid = i;
    //printf("seted?%p and pid %d\n", playerArray[i]->gameHandler, playerArray[i]->pid);

    // 通知玩家设定用户名。
    char tmpstr[256];
    sprintf(tmpstr, "[Info ] [Server] Welcome to the Blood On The Clocktower server!.\n[Info ] [Server] Please set your username by: set username <your name>.\n");
    
    sendSocket(i, tmpstr);
    //printf("[Info ] [Server] New player created.%d\n", (long)playerArray[i]);
    return 0;
}

// deprecated
int GameHandler::delUser(int fid){
    int i;
    for(i = 0;i < MAX_PLAYERS;i++){
        if(playerArray[i]->fid == fid){
            break;
        }
    }
    if(i == MAX_PLAYERS){
        return -1;
    }
    delete playerArray[i];
    playerArray[i] = NULL;
    return 0;
}

int GameHandler::newConnection(int fid, int type){
    // find unused uid.
    int uid;
    for(uid = 0; uid < MAX_PLAYERS; ++uid){
        if(playerArray[uid] == NULL){
            break;
        }
    }
    if(uid == MAX_PLAYERS){
        return -1;
    }

    // new player object
    Player* tmp = new Player(this);
    playerArray[uid] = tmp;
    // set uid, fid, type.
    tmp->uid = uid;
    tmp->fid = fid;
    tmp->connectionType = type;

    // notify player to set username.
    char msgstr[256];
    sprintf(msgstr, "[Info ] [Server] Welcome to the Blood On The Clocktower server!.\n");
    sendSocket(uid, msgstr);
    sprintf(msgstr, "[Info ] [Server] Please set your username by: <set username [your name]>.\n");
    sendSocket(uid, msgstr);

    return 0;
}

int GameHandler::closeConnection(int fid, int type){
    // find user uid
    int uid;
    Player* tp;
    for(uid = 0; uid < MAX_PLAYERS; ++uid){
        tp = playerArray[uid];
        if(tp->fid == fid && tp->connectionType == type){
            break;
        }
    }
    if(uid == MAX_PLAYERS){
        return -1;
    }

    if(tp->joinedGame == -1){
        delete tp;
        playerArray[uid] = NULL;
    } else {
        gameArray[tp->joinedGame]->playerArray[tp->pid] = PID_PLAYER_QUIT_GAME;
        delete tp;
        playerArray[uid] = NULL;
    }

    return 0;
}

int GameHandler::recvSocket(int uid, char* data, int len){

    // 这里先简单的写个广播试试。
    // for(int i = 0;i < MAX_PLAYERS;i++){
    //     if(playerArray[i] != NULL){
    //         sendSocket(playerArray[i]->fid, data, len);
    //     }
    // }

    
    router(uid, data, len);
    return 0;
}

// type is 1 or 2. send(fid, msg)
int GameHandler::sendSocket(int uid, char* data){
    if(playerArray[uid]->connectionType == 1){
        int len = strlen(data);
        socketHandler->sendSocket(playerArray[uid]->fid, data, len);
        //_sendSocket(uid, data, len);
        return 0;
    } else if (playerArray[uid]->connectionType == 2){
        //int len = strlen(data);
        webServer->sendWebSocket(playerArray[uid]->fid, data);
        return 0;
    }
     
    return 0;
}

int GameHandler::router(int uid, char* data, int len) {
    char buf[256];
    strcpy(buf, data);
    int tmplen = strlen(buf);
    buf[tmplen-1] = '\0';
    const char sstrip[2] = " ";

    char* command;
    uint64 input;
    char backStr[256];

    command = strtok(buf, sstrip);
    //printf("dbg %p %s.\n", command, command);
    if(command == NULL){
        printf("[Error] [Server] Received null ptr!\n");
        sprintf(backStr, "[Error] [Server] Sent null str!\n");
        sendSocket(uid, backStr);
    } else if (strcmp(command, "debug") == 0) {
        // get token
        command = strtok(NULL, sstrip);
        
        if(command == NULL){
            printf("[Info ] [Client] Clint - %d attempt to gain admin rights without entering token.\n", uid);
            sprintf(backStr, "[Error] [Server] Please input the token!\n");
            sendSocket(uid, backStr);
        } else {
            sscanf(command, "%llx", &input);
            if(input == 0x1234) { // 密码：0x1234
                playerArray[uid]->isDeveloper = 1;
                printf("[Info ] [Client] Client - %d get into debug mode.\n", uid);
                sprintf(backStr, "[Info ] [Server] OK!\n");
                sendSocket(uid, backStr);
            } else {
                printf("[Info ] [Client] Clint - %d attempt to gain admin rights with wrong token.\n", uid);
                sprintf(backStr, "[Error] [Server] Wrong password!\n");
                sendSocket(uid, backStr);
            }
        }
    } else if (strcmp(command, "startgame") == 0) {
        // get token
        input = newGame(uid);
        if(input < 0){
            sprintf(backStr, "[Error] [Server] Start new game failed!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "joingame") == 0) {
        // get token
        command = strtok(NULL, sstrip);
        if(command == NULL){
            printf("[Info ] [Client] Clint - %d attempt to join a room but inputed empty code.\n", uid);
            sprintf(backStr, "[Error] [Server] Please input the code!\n");
            sendSocket(uid, backStr);
        } else {
            sscanf(command, "%llx", &input);

            int ret = playerArray[uid]->joinGame(input);
            if(ret == 0){
                sprintf(backStr, "[Info ] [Server] Join room succeed!\n");
            } else {
                sprintf(backStr, "[Error] [Server] Join room failed \n");
            }
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "send") == 0) {
        // 获取接收者pid
        command = strtok(NULL, sstrip);
        int t1 = playerArray[uid]->joinedGame; 
        int c1 = t1 < 0; // 玩家必须加入房间才能发送消息
        
        if(command == NULL || c1){
            printf("[Info ] [Client] Clint - %d attempt to send message without pid.\n", uid);
            sprintf(backStr, "[Error] [Server] Please input the legal pid!\n");
            sendSocket(uid, backStr);
        } else {
            sscanf(command, "%llu", &input);
            int c2 = (input < 0) || (input > (MAX_PLAYERS_PER_GAME + 1)); // 约束被发送对象的pid
            int t2 = playerArray[uid]->joinedGame; // 玩家加入的房间id
            int c3 = gameArray[t2]->playerArray[input] < 0; // 必须发送给存在的玩家。
            
            if(c2 || c3){
                printf("[Info ] [Client] Clint - %d attempt to send message with illegal pid.\n", uid);
                sprintf(backStr, "[Error] [Server] Please input the legal pid!\n");
                sendSocket(uid, backStr);
            } else {
                // 获取发送的数据
                command = strtok(NULL, sstrip);
                sprintf(backStr, "[Info ] [Server] Sent!\n");
                sendSocket(uid, backStr);

                char msg[256];
                sprintf(msg, "[Smsg ] [Client] PlayerPID - %d said: %s\n", uid, command);
                int t3 = gameArray[t2]->playerArray[input];
                sendSocket(t3, msg); // 
            }
        }
    } else if (strcmp(command, "sendall") == 0) {
        // 获取接收者pid
        command = strtok(NULL, sstrip);
        int t1 = playerArray[uid]->joinedGame; 
        int c1 = t1 < 0; // 玩家必须加入房间才能发送消息
        
        if(command == NULL || c1){
            printf("[Info ] [Client] Clint - %d attempt to sendall without input message.\n", uid);
            sprintf(backStr, "[Error] [Server] Please input the legal msg!\n");
            sendSocket(uid, backStr);
        } else {
            int t2 = playerArray[uid]->joinedGame; // 玩家加入的房间id
            char msg[256];
            sprintf(msg, "[Amsg ] [Client] PlayerPID - %d said: %s\n", uid, command);
            for(int i = 0;i <= MAX_PLAYERS_PER_GAME; i++) {
                int t3 = gameArray[t2]->playerArray[i];
                int c2 = t3 >= 0; // 该玩家必须存在
                if(c2){
                    sendSocket(t3, msg);
                }
            }
            sprintf(backStr, "[Info ] [Server] Sent!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "showplayers") == 0) {
        // 首先要求玩家必须加入游戏
        int t1 = playerArray[uid]->joinedGame;
        if(t1 < 0){
            sprintf(backStr, "[Error] [Server] Please join room first!\n");
            sendSocket(uid, backStr);
        } else {
            int t2 = playerArray[uid]->joinedGame; // 玩家加入的房间id
            char msg[1024];
            sprintf(msg, "[Info ] [Server] (uid - pid - cid - uname \\n)\n");
            //sendSocket(uid, msg);
            // todo
            // strcat(dest, src);  |This is destinationThis is source|
            for(int i = 0;i < MAX_PLAYERS_PER_GAME; i++){
                char msg2[256];
                int t3 = gameArray[t2]->playerArray[i];
                if(t3 < 0){
                    continue;
                }
                sprintf(msg2, "%d %d %lu %s\n", t3, playerArray[t3]->pid, playerArray[t3]->characterID, playerArray[t3]->username);
                strcat(msg, msg2);
            }

            sendSocket(uid, msg);


        }
    } else if (strcmp(command, "poll") == 0) {
        // 带票，不过具体的逻辑判断过程交给Game内部了，这里只是去做个标记
        command = strtok(NULL, sstrip);
        if(command == NULL){
            printf("[Info ] [Client] Clint - %d argument error.\n", uid);
            sprintf(backStr, "[Error] [Server] Please input the pid!\n");
            sendSocket(uid, backStr);
        } else {
            sscanf(command, "%llu", &input);
            playerArray[uid]->isInitiateVoting = 1;
            playerArray[uid]->votingPerson = input;
            sprintf(backStr, "[Info ] [Server] Succeed!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "vote") == 0) {
        // 同上，投票
        command = strtok(NULL, sstrip);
        if(command == NULL){
            printf("[Info ] [Client] Clint - %d argument error.\n", uid);
            sprintf(backStr, "[Error] [Server] Please input the argument!\n");
            sendSocket(uid, backStr);
        } else {
            sscanf(command, "%llu", &input);
            playerArray[uid]->isParticipateVoting = input;
            sprintf(backStr, "[Info ] [Server] Succeed!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "useskillsp") == 0) {
        // 特殊技能，如士兵刀恶魔
        command = strtok(NULL, sstrip);
        if(command == NULL){
            printf("[Info ] [Client] Clint - %d argument error.\n", uid);
            sprintf(backStr, "[Error] [Server] Please input the argument!\n");
            sendSocket(uid, backStr);
        } else {
            sscanf(command, "%llu", &input);
            playerArray[uid]->skillArgSP = input;
            sprintf(backStr, "[Info ] [Server] Succeed!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "skipstatus") == 0) {
        // 跳过状态，或准备
        playerArray[uid]->isSkipStatus = 1;
        sprintf(backStr, "[Info ] [Server] Succeed!\n");
        sendSocket(uid, backStr);
    } else if (strcmp(command, "kpc") == 0) {
        // kp
        command = strtok(NULL, sstrip);
        if(command == NULL){
            printf("[Info ] [Client] Clint - %d attempt to gain admin rights without entering token.\n", uid);
            sprintf(backStr, "[Error] [Server] Please input the token!\n");
            sendSocket(uid, backStr);
        } else {
            // kp的相关操作也留给Game内部完成了，这里仅作标记。毕竟kp没什么技能，这里就用两个skillArg变量来分别储存pid和变量了。
            // 考虑到程序运行应该是比kp做出操作快的，所以就不管其他情况了。
            sscanf(command, "%llu", &input);
            playerArray[uid]->skillArg = input;

            command = strtok(NULL, sstrip);
            if(command){
                sscanf(command, "%lu", &(playerArray[uid]->skillArgSP));
            }
        }
    } else if (strcmp(command, "confirm") == 0) {
        // 这里也是，就直接用 isSkipStatus 这个变量了
        playerArray[uid]->isSkipStatus = 1;
        sprintf(backStr, "[Info ] [Server] Succeed!\n");
        sendSocket(uid, backStr);
    } else if (strcmp(command, "setkp") == 0) {
        int ret = playerArray[uid]->joinKP();
        if(ret == 0){
            sprintf(backStr, "[Info ] [Server] Succeed!\n");
            sendSocket(uid, backStr);
        } else {
            sprintf(backStr, "[Error] [Server] Failed!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "removekp") == 0) {
        int ret = playerArray[uid]->quitKP();
        if(ret == 0){
            sprintf(backStr, "[Info ] [Server] Succeed!\n");
            sendSocket(uid, backStr);
        } else {
            sprintf(backStr, "[Error] [Server] Failed!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "eggs") == 0) {
        sprintf(backStr, "[Eggs ] [Server] Wow! You find the easter eggs!\n");
        sendSocket(uid, backStr);
    } else if (strcmp(command, "sendteam") == 0) {
        command = strtok(NULL, sstrip);
        int ret = playerArray[uid]->chat(MAX_PLAYERS_PER_GAME+3, command);
        if(ret == 0){
            sprintf(backStr, "[Info ] [Server] Succeed!\n");
            sendSocket(uid, backStr);
        } else {
            sprintf(backStr, "[Error] [Server] Failed!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "quitgame") == 0) {
        command = strtok(NULL, sstrip);
        int ret = playerArray[uid]->quitGame();
        if(ret == 0){
            sprintf(backStr, "[Info ] [Server] Succeed!\n");
            sendSocket(uid, backStr);
        } else {
            sprintf(backStr, "[Error] [Server] Failed!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "set") == 0) {
        command = strtok(NULL, sstrip);
        if(command == NULL){
            printf("[Info ] [Client] Clint - %d wrong argument!\n", uid);
            sprintf(backStr, "[Error] [Server] Please input the legal argument!\n");
            sendSocket(uid, backStr);
        } else if (strcmp(command, "username") == 0) {
            command = strtok(NULL, sstrip);
            if(command){
                strcpy(playerArray[uid]->username, command);
                sprintf(backStr, "[Info ] [Server] Succeed!\n");
                sendSocket(uid, backStr);
            } else {
                sprintf(backStr, "[Error] [Server] Failed!\n");
                sendSocket(uid, backStr);
            }
        } else if (strcmp(command, "skillarg") == 0) {
            sscanf(command, "%lld", &input);
            playerArray[uid]->skillArg = input;
            if(1){
                sprintf(backStr, "[Info ] [Server] Succeed!\n");
                sendSocket(uid, backStr);
            } else {
                sprintf(backStr, "[Error] [Server] Failed!\n");
                sendSocket(uid, backStr);
            }
        } else if (strcmp(command, "timeout") == 0) {
            sprintf(backStr, "[Error] [Server] Not support in alpha version!\n");
            sendSocket(uid, backStr);
        } else {
            sprintf(backStr, "[Error] [Server] Unknown varible!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "get") == 0) {
        command = strtok(NULL, sstrip);
        if(command == NULL){
            printf("[Info ] [Client] Clint - %d wrong argument!\n", uid);
            sprintf(backStr, "[Error] [Server] Please input the legal argument!\n");
            sendSocket(uid, backStr);
        } else if (strcmp(command, "timeout") == 0) {
            int gameID = playerArray[uid]->joinedGame;
            if(gameID < 0){
                sprintf(backStr, "[Error] [Server] You not joined a room!\n");
                sendSocket(uid, backStr);
            } else {
                sprintf(backStr, "[Info ] [Server] Phase end after [%ld] second(s).\n", gameArray[gameID]->getRemainTime());
                sendSocket(uid, backStr);
            }
        } else if (strcmp(command, "timeout2") == 0) {
            sprintf(backStr, "[Error] [Server] Not support in alpha version!\n");
            sendSocket(uid, backStr);
        } else {
            sprintf(backStr, "[Error] [Server] Unknown varible!\n");
            sendSocket(uid, backStr);
        }
    } else if (strcmp(command, "help") == 0) {
        //printf("[Info ] [Client] Clint - %d attempt to gain admin rights without entering token.\n", uid);
        sprintf(backStr, "[Info ] [Server] Trying to send something but there's nothing here in alpha version.\n");
        sendSocket(uid, backStr);
    } else {
        sprintf(backStr, "[Error] [Server] Command not found!\n");
        sendSocket(uid, backStr);
    }

}

int GameHandler::newGame(int creator_uid){
    int i;
    for(i = 0;i < MAX_GAMES;i++){
        if(gameArray[i] == NULL){
            break;
        }
    }
    if(i == MAX_GAMES){
        return -1;
    }
    
    Game *tmp = new Game(this);
    //printf("4444%p\n", this);
    gameArray[i] = tmp;
    gameArray[i]->thread_id = i;

    printf("[Info ] [Server] Game code: %x\n", tmp->code);
    char msgstr[256];
    sprintf(msgstr, "[Info ] [Server] Room created succeed with the code: %x.\n", tmp->code);
    sendSocket(creator_uid, msgstr);
    playerArray[creator_uid]->joinGame(tmp->code);

    int rc = pthread_create(&tmp->run_thread, NULL, startGame, (void *)gameArray[i]);
    if (rc) {
        printf("[Error] \n");
    }
    

    return i;
}

void* startGame(void* game_ctx){  
    // 对传入的参数进行强制类型转换，由无类型指针变为整形数指针，然后再读取
    Game* ctx = (Game*)game_ctx;
    ctx->run();
    //int exit_game = 0;
    //for(int i = 0;i < 100;i++){
    //    exit_game = ctx->frame();
    //    if(exit_game < 0){
    //        break;
    //    }
    //}

    // for player in game: quitgame.
    // tid = game->tid
    // delete game_ctx
    // gameArray[tid] = NULL;

    printf("[Info ] [Server] Game thread %d exited.\n", ctx->thread_id);
    return NULL;
}

int GameHandler::initWebServer(){
    webServer = new WebServer(this);

    return 0;
}

int GameHandler::startWebServer(){
    int rc = pthread_create(&webServer->run_thread, NULL, _startWebServer, (void *)webServer);
    if (rc) {
        printf("[Error] \n");
    }

    return 0;
}

void* _startWebServer(void* webserver_ctx){
    WebServer* ctx = (WebServer*)webserver_ctx;
    ctx->start();
    return NULL;
}