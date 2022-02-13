

#include <stdio.h>
#include <./GameHandler.h>
#include <./Player.h>

Player::Player(GameHandler* _gameHandler){
    test = 0;
    fid = 0;
    uid = 0;
    pid = -1;
    isDeveloper = 0;

    gameHandler = _gameHandler;

    joinedGame = -1;
    sprintf(username, "undefined");
}

int Player::joinGame(int code){
    int i;
    printf("[d]%p \n", gameHandler);
    printf("[d]%p \n", gameHandler->gameArray[0]);
    printf("[d]%x \n", gameHandler->gameArray[0]->code);
    printf("[d]%x \n", code);
    for(i = 0;i < MAX_GAMES;i++){
        //printf("%p ", gameHandler->gameArray[0]);
        if(gameHandler->gameArray[i] == NULL){
            //printf("xkxkxk ");
            continue;
        }
        printf("dfdfdf\n");
        if(gameHandler->gameArray[i]->code == code){
            joinedGame = i;
            gameHandler->gameArray[i]->playerJoin(uid);
            printf("[d] success\n");
            return 0;
        }
    }
    printf("fffffff\n");

    return -1;
}

int Player::quitGame(){
    if(joinedGame == -1){
        return -1;
    }
    if(gameHandler->gameArray[joinedGame] == NULL){
        return -2;
    }
    gameHandler->gameArray[joinedGame]->playerQuit(uid);
    joinedGame = -1;
    return 0;
}

Game* Player::getJoinedGame() {
    if(joinedGame == -1){
        return NULL;
    }
    return gameHandler->gameArray[joinedGame];
}

int Player::joinKP(){
    return 0;
}

int Player::quitKP(){
    return 0;
}



int Player::chat(int to, char* data){
    // 0 - 19: player. 20: kp. 21: all. 22: team


    return 0;
}