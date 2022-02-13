#ifndef _GAME
#define _GAME

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>

#include <./Player.h>

#define _for(i,a,b) for( int i=(a); i<(b); ++i)
// Skip non-existing players to avoid Segmentation fault.
#define skipNP(i) if(playerArray[(i)] < 0){continue;}


const int MAX_PLAYERS_PER_GAME = 20;

const int TOTAL_DEMONS_NUM = 1;
const int TOTAL_MINIONS_NUM = 4;
const int TOTAL_OUTSIDERS_NUM = 4;
const int TOTAL_TOWNSFOLK_NUM = 13;

const int PID_PLAYER_DOES_NOT_EXIST = -1;
const int PID_PLAYER_QUIT_GAME = -2;

class GameHandler;
class Player;

enum wait_time {
    WAIT_GAME_BEGIN             = 0,
    WAIT_KP_CHANGE_CHARACTER    = 1,
    WAIT_PLAYER_USE_SKILL       = 2,
    WAIT_PLAYER_CHATTING_END    = 3,
    WAIT_KP_CHANGE_ARGUMENT     = 4,
    WAIT_PLAYER_POLL            = 5,
    WAIT_PLAYER_VOTE            = 6
};

enum characters {
    // Townsfolk 好人 13个
    C_WASHERWOMAN=101, // 洗衣妇 在游戏开始时，你会得知两名玩家其中一位的村民身份。
    C_LIBRARIAN, // 图书管理员 在游戏开始时，你会得知两名玩家其中一位外来者的身份，或得知本局游戏有没有外来者。
    C_INVESTIGATOR, // 调查员 在游戏开始时，你会得知两名玩家其中一位爪牙的身份。
    C_CHEF, // 厨师 在游戏开始时，你会得知有多少邪恶玩家相邻。
    C_EMPATH, // 共情者 每个夜晚，你会得知与你邻座的存活玩家有几个是邪恶的。
    C_FORTUNE_TELLER, // 占卜师，每个夜晚，选择两名玩家：你会得知这两名玩家中是否有恶魔；但有一位善良玩家也被你的技能视为恶魔。
    C_UNDERTAKER, // 掘墓人，每个夜晚*，你会得知今天因处决而死去的玩家的身份。
    C_MONK, // 僧侣，每个夜晚*，选择一名玩家（除了自己）：恶魔的能力在今晚不会对那名玩家生效。
    C_RAVENKEEPER, // 训鸦人 如果你在晚上死去，你会被唤醒并选择一名玩家：你会得知那名玩家的身份。
    C_VIRGIN, // 处女，当你第一次被提名时，如果提名者是村民，那么那名玩家立刻被处决。
    C_SLAYER, // 恶魔猎手，每局游戏限一次，在白天时，你可以公开选择一名玩家：如果那名玩家是恶魔，那么该玩家会死去。
    C_SOLDIER, // 士兵 恶魔的能力对你无效。
    C_MAYOR, // 市长 如果在白天只有三名玩家存活并且没有执行处决，你的阵营获胜。如果你在夜晚死去，另一位玩家可能会替代你死去。

    // Outsiders 外来者 4个
    C_BUTLER=201, // 管家 每个夜晚，选择一名玩家（除了自己）：民屯田你只能在那名玩家参与投票的处决中投票。
    C_DRUNK, // 酒鬼 你喝醉了，你以为你是一个其他的村民身份，但实际上你不是。
    C_RECLUSE, // 隐士 你可能会被判定为一名邪恶玩家（爪牙或恶魔），即使你已经死亡。
    C_SAINT, // 圣徒 如果你因为处决而死亡，你的阵营失败。

    // Minions 坏人 4个
    C_POISONER=301, // 投毒者 每个夜晚，选择一名玩家：那名玩家今晚和明天白天会中毒。
    C_SPY, // 间谍 每个夜晚，你可以查看剧本真相。你可能会被判定为一名善良玩家（村民或外来者），即使你已经死亡。
    C_SCARLET_WOMAN, // 猩红夫人 如果有五名或更多玩家存活且恶魔死去时，你会成为恶魔。（旅行者的数量不算计在内）。
    C_BARON, // 场上会有额外两名外来者（外来者+2）。

    // Demons 恶魔 1个
    C_IMP=401 // 小恶魔 每个夜晚*，选择一名玩家：杀死那名给玩家。如果你用这种方式自杀，一位爪牙会变成小恶魔。
};

class Game {
    public:
    int test;
    int code;               // 加入房间所需的代码（密码）
    int playerNum;          // 房间内玩家数量，玩家进入/退出游戏时自动增减
    int gameStatus;         // 游戏是处在白天/夜晚/等待中
    __uint64_t frameArg;
    time_t endTime;
    int timeOut[16];
    int isDestroy; // 自毁标志，如果此处置1，在此frame结束后将会自动退出线程，释放资源。

    int playerArray[MAX_PLAYERS_PER_GAME + 1];
    int playerKP;

    pthread_t run_thread; // 这个房间运行的线程。
    int thread_id;

    GameHandler* gameHandler;

    Game(GameHandler* gameHandler);

    Player* getPlayerByPID(int pid);
    Player* getPlayerByUID(int uid);
    Player* getPlayerKP();
    int findPlayerByCharacter(int cid);

    int playerJoin(int uid);
    int playerQuit(int uid);
    int setKP(int pid);
    int removeKP();

    int sendMsgAll(char* msg, int len);
    int sendMsg(int pid, const char* format,...);
    int recvMsg();
    time_t getRemainTime();


    int AssignCharacters(); // 给玩家分配职业

    int isExistKP();
    int isNowChatable(int pid);
    int isPlayerDeath(int pid);
    int isPlayerDrunk(int pid);
    int isPlayerPoisoned(int pid);
    int isPlayerProtectedByMonk(int pid);
    int isPlayerVotable4Bulter(int pid);
    int isPlayerBotable(int from, int to);

    int init();
    int killPlayer(int pid);
    int useSkill(int pid);
    int run(); // 试试新的游戏实现方式。
    int frame(); // 这个函数就是在线程中被 while(1) 的函数啦。

    int selfDestroy(); // 相当于添加一个标记，game
};

void swap(int &a, int &b);

#endif