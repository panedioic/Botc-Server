#ifndef _PLAYER
#define _PLAYER

#include <stdio.h>
#include <stdint.h>

#include <./GameHandler.h>
#include <./Game.h>

class GameHandler;

class Player {
    public:
    int test;
    GameHandler* gameHandler;

    int connectionType;                     // using RawSocket / WebSocket (1 / 2)
    int fid;                                // 这个 Player 对应的玩家与服务器的连接的套接字的描述符。
    // void* cptr;                          // if websocket, pointer of connection.
    int uid;                                // 这个 Player 在 GameHandler 的 playerArray 中的下标。
    int pid;                                // 这个 Player 在加入的房间中的次序号。
    char username[32];
    int isDeveloper;

    int joinedGame;                         // 玩家加入的房间的房间号

    uint64_t characterID;
    
    uint64_t isSkipStatus;                  // 跳过状态，（如讨论时间所有人都决定跳过后直接进入下一环节）
    uint64_t isInitiateVoting;              // 是否发起投票                                           (deprecated)
    uint64_t isParticipateVoting;           // 是否参与投票 | 1 - 踢出 | 2 - 弃票 |                    (deprecated)   
    uint64_t votingPerson;                  // 被投票的人                                             (deprecated)
    uint64_t skillArg;
    uint64_t skillArgSP;                    // 特殊技能参数，如士兵刀恶魔
    uint64_t isDrunk;                       // 
    uint64_t deathDate;                     // The date of death, if equal to 0 it is not yet dead
    uint64_t isSkillUsed;
    uint64_t isVotedAfterDeath;

    // skillused



    Player(GameHandler* gameHandler);

    int joinGame(int code);
    int quitGame();
    int joinKP();
    int quitKP();

    int chat(int to, char* data);

    Game* getJoinedGame();
};

#endif