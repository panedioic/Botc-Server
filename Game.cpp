

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdarg.h>

#include <./Game.h>

const uint64 ONCE_CHECK = (1UL << 56);

void swap(int &a, int &b)
{
    int temp = a;
    a = b;
    b = temp;
}

Game::Game(GameHandler* _gameHandler){
    test = 0;
    code = rand() & 0xffffffff;
    playerNum = 0;
    gameStatus = 2;
    gameHandler = _gameHandler;
    frameArg = ONCE_CHECK;

    for(int i = 0;i < MAX_PLAYERS_PER_GAME+1;i++){
        playerArray[i] = -1;
    }
    playerKP = -1;

    timeOut[WAIT_GAME_BEGIN         ] = 240;
    timeOut[WAIT_KP_CHANGE_CHARACTER] = 120;
    timeOut[WAIT_PLAYER_CHATTING_END] = 120;
    timeOut[WAIT_PLAYER_USE_SKILL   ] = 120;
    timeOut[WAIT_PLAYER_CHATTING_END] = 120;
    timeOut[WAIT_KP_CHANGE_ARGUMENT ] = 120;
    timeOut[WAIT_PLAYER_POLL        ] = 120;
    timeOut[WAIT_PLAYER_VOTE        ] = 120;
}

Player* Game::getPlayerByPID(int pid){
    if(playerArray[pid] == -1) {
        return NULL;
    }
    if(playerArray[pid] == -2) {
        return NULL;
    }
    return gameHandler->playerArray[playerArray[pid]];
}

Player* Game::getPlayerByUID(int uid){
    //printf("[d] u%d\n", uid);
    //printf("[d] hand - %p\n", gameHandler);
    return gameHandler->playerArray[uid];
    //return NULL;
}

Player* Game::getPlayerKP(){
    if(playerArray[MAX_PLAYERS_PER_GAME] == -1){
        return NULL;
    }
    return gameHandler->playerArray[playerArray[MAX_PLAYERS_PER_GAME]];
}

// 找到某个职业对应的玩家
int Game::findPlayerByCharacter(int cid) {
    int ret = -1;

    if(cid == C_DRUNK){
        for(int i = 0;i < playerNum;i++){
            if(isPlayerDrunk(i)){
                ret = i;
            }
        }
    } else {
        for(int i = 0;i < playerNum;i++){
            if((getPlayerByPID(i)->characterID == cid) && isPlayerDrunk(i) == 0){
                ret = i;
            }
        }
    }

    return ret;
}

int Game::playerJoin(int uid){
    //printf("[b] t1\n");
    if (playerNum >= MAX_PLAYERS_PER_GAME){
        return -1;
    }
    //printf("[b] t3\n");
    for (int i = 0;i < MAX_PLAYERS_PER_GAME; i++){
        if(playerArray[i] == uid){
            return -2;
        }
        //printf("[b] t4\n");
        if(playerArray[i] == -1){
            playerArray[i] = uid;
            getPlayerByUID(uid)->pid = i;
            //printf("[b] t6\n");
            playerNum += 1;
            //printf("[b] t8\n");
            char tmpstr[256];
            //printf("[bbb] t9 %d %p\n", uid, getPlayerByUID(uid));
            sprintf(tmpstr, "[Info ] [Server] New player [%s] joined!\n", getPlayerByUID(uid)->username);
            //printf("[bbb] t7\n");
            int tmplen = strlen(tmpstr);
            sendMsgAll(tmpstr, tmplen);
            break;
        }
        //printf("[b] t5\n");
    }
    //printf("[b] t2\n");
    return 0;
}

int Game::playerQuit(int uid){
    int i;
    for(i = 0;i < MAX_PLAYERS_PER_GAME; i++){
        if(playerArray[i] == uid){
            break;
        }
    }
    if(i == MAX_PLAYERS_PER_GAME){
        return -1;
    }
    if(gameStatus == 2){
        // 非游戏中，可以直接删掉玩家
        for(int j = i;j < playerNum - 1; j++){
            playerArray[j] = playerArray[j+1];
            gameHandler->playerArray[playerArray[j]]->pid = j;
        }
        playerArray[playerNum] = -1;
        playerNum -= 1;
    } else {
        // 游戏中，考虑为玩家自杀，然后把玩家换掉。
        playerArray[i] = -2;
    }
    
    return 0;
}

int Game::setKP(int pid){
    if(playerKP != -1){
        int ret = removeKP();
        if(ret < 0){
            return -1;
        }
    }
    int uid = playerArray[pid];
    playerQuit(playerArray[pid]);
    gameHandler->playerArray[uid]->pid = MAX_PLAYERS_PER_GAME;
    playerKP = uid;

    return 0;

}

int Game::removeKP(){
    if(playerKP == -1){
        return -1;
    }
    if(playerNum == MAX_PLAYERS_PER_GAME){
        return -2;
    }
    int uid = playerKP;
    playerKP = -1;
    playerJoin(uid);
}

int Game::sendMsgAll(char* msg, int len){
    for(int i = 0;i <= MAX_PLAYERS_PER_GAME; i++){
        if(playerArray[i] == -1){
            continue;
        }
        printf("[dd] 2333 %d\n", i);
        gameHandler->sendSocket(getPlayerByPID(i)->uid, msg);
    }
    return 0;
}

int Game::sendMsg(int pid,  const char* format,...){
    if(pid == PID_PLAYER_DOES_NOT_EXIST){
        return -1;
    }
    if(pid == PID_PLAYER_QUIT_GAME){
        return -2;
    }
    if(pid < 0 || pid > MAX_PLAYERS_PER_GAME){
        return -3;
    }
    va_list args;
    char msgstr[1024];
 
    va_start(args,format);
    vsprintf(msgstr,format,args);
    va_end(args);

    gameHandler->sendSocket(playerArray[pid], msgstr);

    return 0;
}

time_t Game::getRemainTime(){
    return endTime - time(NULL);
}

int Game::AssignCharacters(){
    // 无男爵时各阵营人数
    // 恶魔 爪牙 外来者 村民 | 行数+4为人数
    int numberList[] = {
        0, 0, 0, 0, // padding
        0, 0, 0, 1, //1 players(padding)
        1, 0, 0, 1, //2 players(debug)
        1, 0, 0, 2, //3 players(debug)
        1, 0, 0, 3, //4 players(debug)
        1, 1, 1, 2, //5 players
        1, 1, 1, 3, //6 players
        1, 1, 0, 5, //7 players
        1, 1, 1, 5, //8 players
        1, 1, 2, 5, //9 players
        1, 2, 0, 7, //10 players
        1, 2, 1, 7, //11 players
        1, 2, 2, 7, //12 players
        1, 3, 0, 9, //13 players
        1, 3, 1, 9, //14 players
        1, 3, 2, 9, //15 players
    };

    // 随机是否存在男爵
    int existBaron = rand() % 2;
    if(playerNum <= 5){
        existBaron = 0; // 人数太少时不能存在男爵。
    }

    int DemonsCount     = numberList[playerNum * 4 + 0];
    int MinionsCount    = numberList[playerNum * 4 + 1];
    int OutsidersCount  = numberList[playerNum * 4 + 2];
    int TownsfolkCount  = numberList[playerNum * 4 + 3];
    
    if(existBaron){
        MinionsCount    -= 1; // 已经存在男爵，可以少抽一个狼。
        OutsidersCount  += 2;
        TownsfolkCount  -= 2;
    }

    int DemonsList[1] = {401};
    int MinionsList[4] = {301, 302, 303, 304};
    int OutsidersList[4] = {201, 202, 203, 204};
    int TownsfolkList[13] = {101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113};

    // 洗牌
    for (int i = 1 - 1; i > 0; --i){ swap(DemonsList[i], DemonsList[rand() % (i + 1)]) ;}
    for (int i = 4 - 1; i > 0; --i){ swap(MinionsList[i], MinionsList[rand() % (i + 1)]) ;}
    for (int i = 4 - 1; i > 0; --i){ swap(OutsidersList[i], OutsidersList[rand() % (i + 1)]) ;}
    for (int i = 13 - 1; i > 0; --i){ swap(TownsfolkList[i], TownsfolkList[rand() % (i + 1)]) ;}


    int tmpCharactersList[20];
    int cnt = 0;
    for(int i = 0; i < DemonsCount      ; ++i){ tmpCharactersList[i+cnt] = DemonsList[i];}
    cnt += DemonsCount;
    for(int i = 0; i < MinionsCount     ; ++i){ tmpCharactersList[i+cnt] = MinionsList[i];}
    cnt += MinionsCount;
    for(int i = 0; i < OutsidersCount   ; ++i){ tmpCharactersList[i+cnt] = OutsidersList[i];}
    cnt += OutsidersCount;
    for(int i = 0; i < TownsfolkCount   ; ++i){ tmpCharactersList[i+cnt] = TownsfolkList[i];}
    cnt += TownsfolkCount;
    
    // 如果之前随机到存在男爵，那在爪牙中就必须有男爵，如果之前的洗牌没有抽到男爵就给其中一个人换成男爵。
    if(existBaron){
        existBaron = 0;
        for(int i = playerNum - 1; i > 0; --i){
            if(tmpCharactersList[i] == C_BARON){
                existBaron = 1;
            }
        }
        // 因为前面的逻辑是有男爵的话就直接少抽一个，所以这里需要多补角色
        // 如果已经抽到了男爵，就补一个其他爪牙；如果没抽到，就补上。
        if(existBaron){
            tmpCharactersList[playerNum-1] = MinionsList[TOTAL_MINIONS_NUM-1];
        } else {
            tmpCharactersList[playerNum-1] = C_BARON;
        }
    } else {
        // 若没有男爵但被抽到了则把他换掉
        for(int i = playerNum; i > 0; --i){
            if(tmpCharactersList[i] == C_BARON){
                tmpCharactersList[i] = MinionsList[TOTAL_MINIONS_NUM-1];
            }
        }
    }

    // 最后再洗一次牌，之后分配给玩家。
    for(int i = playerNum - 1; i > 0; --i){
        swap(tmpCharactersList[i], tmpCharactersList[rand() % (i + 1)]);
    }
    for(int i = 0; i < playerNum; ++i){
        getPlayerByPID(i)->characterID = tmpCharactersList[i];
    }

    //还得处理下酒鬼的问题。
    // 酒鬼 = C_DRUNK = 202.
    int drunkID = -1;
    for(int i = 0;i < playerNum;i++){
        if(tmpCharactersList[i] == C_DRUNK){
            drunkID = i;
        }
    }
    if(drunkID >= 0){
        int newCharacter = rand() % TOTAL_TOWNSFOLK_NUM;
        newCharacter += 101;

        getPlayerByPID(drunkID)->characterID = newCharacter;
        getPlayerByPID(drunkID)->isDrunk = 1;
    }

    // 告诉恶魔三个不存在的职业（存于 skillarg）
    int demonID = -1;
    for(int i = 0;i < playerNum; ++i){
        if(getPlayerByPID(i)->characterID == C_IMP){
            getPlayerByPID(i)->skillArgSP = 1;
            getPlayerByPID(i)->skillArgSP += ((uint64_t)TownsfolkList[TOTAL_TOWNSFOLK_NUM-1]<<48);
            getPlayerByPID(i)->skillArgSP += ((uint64_t)TownsfolkList[TOTAL_TOWNSFOLK_NUM-2]<<32);
            getPlayerByPID(i)->skillArgSP += ((uint64_t)TownsfolkList[TOTAL_TOWNSFOLK_NUM-3]<<16);
        }
    }
    return 0;

}

int Game::isExistKP(){
    if(playerArray[MAX_PLAYERS_PER_GAME] == -1){
        return 0;
    }
    return 1;
}

int Game::isNowChatable(int pid){ // later
    return 1;
}

int Game::isPlayerDeath(int pid){
    if(playerArray[pid] < 0){
        return 1;
    }
    if(getPlayerByPID(pid)->deathDate > 0){
        return 1;
    }
    return 0;
}

int Game::isPlayerDrunk(int pid){
    int ret = getPlayerByPID(pid)->isDrunk;
    return ret;
}

int Game::isPlayerPoisoned(int pid){
    int t1 = findPlayerByCharacter(C_POISONER);
    if(t1 < 0){
        return 0;
    }
    int t2 = getPlayerByPID(t1)->skillArg;
    int t3 = (t2 >> 8) & 0xff;
    int t4 = t2 & 0xff;

    if((t3 == pid) && (t4 == 1)){
        return 1;
    } else {
        return 0;
    }
}

int Game::isPlayerProtectedByMonk(int pid){
    int monkspid = findPlayerByCharacter(C_MONK);
    if(monkspid < 0){
        return 0;
    }
    int monksskillarg = getPlayerByPID(monkspid)->skillArg;
    int skillarg1 = (monksskillarg >> 8) & 0xff;
    int skillarg2 = monksskillarg & 0xff;
    int ismonkbepoisoned = isPlayerPoisoned(monkspid);

    if((skillarg1 == pid) && (skillarg2 == 1) && (!ismonkbepoisoned)){
        return 1;
    }
    return 0;
}

int Game::isPlayerVotable4Bulter(int pid){
    int bulterpid = findPlayerByCharacter(C_MONK);
    if(bulterpid < 0){
        return 0;
    }
    int bulterskillarg = getPlayerByPID(bulterpid)->skillArg;
    int skillarg1 = (bulterskillarg >> 8) & 0xff;
    int skillarg2 = bulterskillarg & 0xff;

    if((skillarg1 == pid) && (skillarg2 == 1)){
        return 1;
    }
    return 0;

}

int Game::init(){
    return 0;
}

int Game::killPlayer(int pid){
    if(playerArray[pid] < 0){
        return -1;
    }
    getPlayerByPID(pid)->deathDate = gameStatus >> 1;
    getPlayerByPID(pid)->isVotedAfterDeath = 0;
    return 0;
}

int Game::useSkill(int pid){
    // 技能使用后，不直接将结果发至玩家，而是由主持人确认之后再发给玩家。主持人确认前，先放进列队中进行等待。
    if(pid == -1){
        return 0; // 没有该职业对应的玩家，直接返回。
    }

    int cid = getPlayerByPID(pid)->characterID; //////////
    // 二次验证，虽然我也不知道有没有必要
    int con1 = isPlayerDeath(pid) == 0; // 玩家是否存活（1为存活）
    int con2 = getPlayerByPID(pid)->isSkillUsed == 0; // 若是一次性技能，玩家是否尚未使用（1为尚未使用）
    int con3 = (gameStatus & 1) == 1; // 是否是夜晚（1为是夜晚）
    int con4 = isPlayerDrunk(pid);
    int con5 = isPlayerPoisoned(pid);
    
    uint64_t arg = getPlayerByPID(pid)->skillArg;
    uint64_t ret = 0; // 后续返回的变量，0为不包含
    
    switch(cid) {
        // 【洗衣妇】【非交互角色】 在游戏开始时，你会得知两名玩家其中一位的村民身份。数据返回格式(uint64)：
        // | 63 - 48  | 47 - 40  | 39 - 32  |  31 - 16  |  15 - 8  |    7 - 0  | （原本打算在这里加上pid，cid和中毒酒鬼之类的来着，不过后来想想还是先算了）
        // |     0    |   pid2   |   pid1   | character |   type   |  状态码(1) | （type: 0 - 正常返回 | 1 - 没有村民）
        case C_WASHERWOMAN: {
            if(con1 && con2 && (!con3)){
                if ( con4 || con5 ) {
                    // 先随便挑两个活人。
                    uint64_t i, j;
                    while(1){
                        i = rand()%playerNum;
                        if((!isPlayerDeath(i)) && (i != pid)){
                            break;
                        }
                    }
                    while(1){
                        j = rand()%playerNum;
                        if((!isPlayerDeath(i)) && (i != pid) && (i != j)){
                            break;
                        }
                    }
                    // 再随便抽个职业。
                    int k = rand()%13+101;
                    // 随机是否存在好人。
                    int l = rand()%2;
                    // 编码
                    ret = (i << 40) + (j << 32) + (k << 16) + (l << 8) + 1;
                } else {
                    // 首先确定游戏中存在好人（若5人局有男爵就全是外来者了）间谍会被认为是好人，返回随机职业。酒鬼和隐士不被认为是村民。
                    int existGoodPerson = 0;
                    for(int i = 0;i < playerNum;i++){
                        if(((getPlayerByPID(i)->characterID / 100 == 1) && (!getPlayerByPID(i)->isDrunk) && (i != pid)) || (getPlayerByPID(i)->characterID == C_SPY)){
                            existGoodPerson = 1;
                            break;
                        }
                    }
                    // 存在好人则可以继续，没有好人就直接返回这游戏没有好人。
                    if(existGoodPerson){
                        uint64 i,j;
                        // 先随机抽一个好人（不包括外来者）作为第一个被选中且返回职业的玩家,再抽取第二个玩家，这个玩家是谁都行。
                        while(1){
                            i = rand() % playerNum;
                            if(((getPlayerByPID(i)->characterID / 100 == 1) && (!getPlayerByPID(i)->isDrunk) && (i != pid)) || (getPlayerByPID(i)->characterID == C_SPY)){
                                break;
                            }
                        }
                        while(1){
                            j = rand() % playerNum;
                            if((j != i) && (i != pid)){
                                break;
                            }
                        }
                        int k = getPlayerByPID(i)->characterID;
                        // 然后随机下这两个玩家的位置。
                        if(rand()%2){
                            int tmp = i;
                            i = j;
                            j = tmp;
                        }
                        // 编码
                        ret = (i << 40) + (j << 32) + (k << 16) + 1;
                    } else {
                        ret = (1 << 8) + 1;
                    }
                    getPlayerByPID(pid)->isSkillUsed = 1;
                }
            }
            break;
        }

        // 【图书管理员】【非交互角色】  在游戏开始时，你会得知两名玩家其中一位外来者的身份，或得知本局游戏有没有外来者。数据返回格式(uint64)：
        // | 63 - 48  | 47 - 40  | 39 - 32  |  31 - 16  |  15 - 8  |    7 - 0  |
        // |     0    |   pid2   |   pid1   | character |   type   |   codr(1) | （type: 0 - 正常返回 | 1 - 没有村民）
        case C_LIBRARIAN:{
            if(con1 && con2 && (!con3)){ // （存活，一次性，白天）
                if ( con4 || con5 ) {
                    // 先随便挑两个活人。
                    uint64_t i, j;
                    while(1){
                        i = rand()%playerNum;
                        if((!isPlayerDeath(i)) && (i != pid)){
                            break;
                        }
                    }
                    while(1){
                        j = rand()%playerNum;
                        if((!isPlayerDeath(i)) && (i != pid) && (i != j)){
                            break;
                        }
                    }
                    // 再随便抽个职业。
                    int k = rand()%4+201;
                    // 随机是否存在外来者。
                    int l = rand()%2;
                    // 编码
                    ret = (i << 40) + (j << 32) + (k << 16) + (l << 8) + 1;
                } else {
                    // 首先确定游戏中存在外来者。这里酒鬼会被认为是外来者，返回酒鬼。
                    int existOutsider = 0;
                    for(int i = 0;i < playerNum;i++){
                        if((getPlayerByPID(i)->characterID / 100 == 2) || (getPlayerByPID(i)->isDrunk) && (i != pid)){
                            existOutsider = 1;
                            break;
                        }
                    }
                    // 存在外来者则可以继续，没有外来者就直接返回这游戏没有外来者。
                    if(existOutsider){
                        uint64 i,j;
                        // 先随机抽一个外来者作为第一个被选中且返回职业的玩家,再抽取第二个玩家，这个玩家是谁都行。
                        while(1){
                            i = rand() % playerNum;
                            if((getPlayerByPID(i)->characterID / 100 == 2) || (getPlayerByPID(i)->isDrunk) && (i != pid)){
                                break;
                            }
                        }
                        while(1){
                            j = rand() % playerNum;
                            if((j != i) && (i != pid)){
                                break;
                            }
                        }
                        int k = getPlayerByPID(i)->characterID;
                        // 然后随机下这两个玩家的位置。
                        if(rand()%2){
                            uint64 tmp = i;
                            i = j;
                            j = tmp;
                        }
                        // 编码
                        ret = (i << 40) + (j << 32) + (k << 16) + 1;
                    } else {
                        ret = (1 << 8) + 1;
                    }
                    getPlayerByPID(pid)->isSkillUsed = 1;
                }
            }
            break;
        }

        // 【调查员】【非交互角色】  在游戏开始时，你会得知两名玩家其中一位爪牙的身份。数据返回格式(uint64)：
        // | 63 - 48  | 47 - 40  | 39 - 32  |  31 - 16  |  15 - 8  |    7 - 0  |
        // |     0    |   pid2   |   pid1   | character |   type   |   code(1) | （type: 0 - 正常返回 | 1 - 没有爪牙）
        case C_INVESTIGATOR: {
            if(con1 && con2 && (!con3)){ // （存活，一次性，白天）
                if ( con4 || con5 ) {
                    // 先随便挑两个活人。
                    uint64_t i, j;
                    while(1){
                        i = rand()%playerNum;
                        if((!isPlayerDeath(i)) && (i != pid)){
                            break;
                        }
                    }
                    while(1){
                        j = rand()%playerNum;
                        if((!isPlayerDeath(i)) && (i != pid) && (i != j)){
                            break;
                        }
                    }
                    // 再随便抽个职业。
                    int k = rand()%4+301;
                    // 随机是否存在爪牙。
                    int l = rand()%2;
                    // 编码
                    ret = (i << 40) + (j << 32) + (k << 16) + (l << 8) + 1;
                } else {
                    // 首先确定游戏中存在爪牙。这里隐士和间谍都会被当成爪牙，间谍会返回间谍。酒鬼隐士不被认为是爪牙。
                    int existMinion = 0;
                    for(int i = 0;i < playerNum;i++){
                        if((getPlayerByPID(i)->characterID / 100 == 3) || (getPlayerByPID(i)->isDrunk) && (i != pid)){
                            existMinion = 1;
                            break;
                        }
                    }
                    // 存在爪牙则可以继续，没有爪牙就直接返回这游戏没有爪牙。
                    if(existMinion){
                        uint64_t i,j;
                        // 先随机抽一个爪牙作为第一个被选中且返回职业的玩家,再抽取第二个玩家，这个玩家是谁都行。
                        while(1){
                            i = rand() % playerNum;
                            if(((getPlayerByPID(i)->characterID / 100 == 3) && (i != pid)) || (getPlayerByPID(i)->characterID == C_RECLUSE)){
                                break;
                            }
                        }
                        while(1){
                            j = rand() % playerNum;
                            if((j != i) && (i != pid)){
                                break;
                            }
                        }
                        int k = getPlayerByPID(i)->characterID;
                        // 然后随机下这两个玩家的位置。
                        if(rand()%2){
                            int tmp = i;
                            i = j;
                            j = tmp;
                        }
                        // 编码
                        ret = (i << 40) + (j << 32) + (k << 16) + 1;
                    } else {
                        ret = (1 << 8) + 1;
                    }
                    getPlayerByPID(pid)->isSkillUsed = 1;
                }
            }
            break;
        }

        // 【厨师】【非交互角色】 在游戏开始时，你会得知有多少邪恶玩家相邻。数据返回格式(uint64)：
        // | 63 - 16  |  15 - 8  |    7 - 0  |
        // |     0    |  number  |   code(1) | 
        case C_CHEF: {
            if(con1 && con2 && (!con3)){ // （存活，一次性，白天）
                if(con4 || con5) {
                    // 随机个数字
                    ret = (rand()%4 << 8) + 1;
                } else {
                    int lastIsBad = 0;
                    int firstIsBad = 0;
                    int neighbors = 0;
                    for(int i = 0;i < playerNum;i++){
                        //int isBad = isBadPerson(cid);
                        // 40x, 30x and reculse is bad.
                        int isBad;
                        int tmpcid = getPlayerByPID(i)->characterID;
                        if(((tmpcid / 100 >= 3) && (tmpcid != C_SPY)) || tmpcid == C_RECLUSE){
                            isBad = 1;
                        } else {
                            isBad = 0;
                        }

                        if (isBad && lastIsBad){
                            ret += (1<<8);
                        }
                        lastIsBad=isBad;
                        if(i==0){
                            firstIsBad = isBad;
                        }
                    }
                    if(lastIsBad && firstIsBad){
                        neighbors += (1<<8);
                    }

                    ret = (neighbors << 8) + 1;
                    getPlayerByPID(pid)->isSkillUsed = 1;
                }
            }
            break;
        }

        // 【共情者】【非交互职业】 每个夜晚，你会得知与你邻座的存活玩家有几个是邪恶的。数据返回格式(uint64)：
        // | 63 - 16  |  15 - 8  |    7 - 0  |
        // |     0    |  number  |   code(1) | 
        case C_EMPATH: {
            if(con1 && con3){ // 活人夜晚
                if (con3 || con4) {
                    ret = (rand()%3 << 8) + 1;
                } else {
                    int k = 0;
                    // 判断顺时针方向是否是坏人
                    for(int i = pid+1;i < playerNum+pid; i++){
                        int j = i;
                        if(j >= playerNum){
                            j -= playerNum;
                        }
                        if(isPlayerDeath(j)){
                            continue;
                        }
                        // k += isBadPerson(getPlayerByPID(j)->characterID);
                        int tmpcid = getPlayerByPID(j)->characterID;
                        int isBad;
                        if(((tmpcid / 100 >= 3) && (tmpcid != C_SPY)) || tmpcid == C_RECLUSE){
                            isBad = 1;
                        } else {
                            isBad = 0;
                        }

                        k += isBad;

                        break;
                    }
                    // 判断逆时针方向是否是坏人
                    for(int i = pid-1;i >= pid-playerNum; i--){
                        int j = i;
                        if(j < 0){
                            j += playerNum;
                        }
                        if(isPlayerDeath(j)){
                            continue;
                        }
                        int tmpcid = getPlayerByPID(j)->characterID;
                        int isBad;
                        if(((tmpcid / 100 >= 3) && (tmpcid != C_SPY)) || tmpcid == C_RECLUSE){
                            isBad = 1;
                        } else {
                            isBad = 0;
                        }

                        k += isBad;
                        break;
                    }
                    ret = (k << 8) + 1;
                }
            }
            break;
        }

        // 【占卜师】【交互职业】 每个夜晚，选择两名玩家：你会得知这两名玩家中是否有恶魔；但有一位善良玩家也被你的技能视为恶魔。数据返回格式(uint64)：
        // | 63 - 32 |  31 - 24  |  23 - 16  |  15 - 8  |    7 - 0  |
        // |    0    |  exist?   |   pid1    |   pid2   |  code(1)  | 
        case C_FORTUNE_TELLER: {
            int isExistBadPerson = 0;
            if(con1 && con3){ // 活人夜晚
                if(con4 || con5){
                    int isExistBadPerson = rand()%2;
                    ret = (isExistBadPerson << 24) + (arg & 0xffff00) + 1;
                } else {
                    int isExistBadPerson = 0;
                    int pid1 = (arg >> 16) & 0xff; // todo
                    int tmpcid1 = getPlayerByPID(pid1)->characterID;
                    if(((tmpcid1 / 100 >= 3) && (tmpcid1 != C_SPY)) || tmpcid1 == C_RECLUSE){
                        isExistBadPerson = 1;
                    }

                    int pid2 = (arg >> 8) & 0xff;
                    int tmpcid2 = getPlayerByPID(pid2)->characterID;
                    if(((tmpcid2 / 100 >= 3) && (tmpcid2 != C_SPY)) || tmpcid2 == C_RECLUSE){
                        isExistBadPerson = 1;
                    }
                    ret = (isExistBadPerson << 24) + (arg & 0xffff00) + 1;
                }
            }
            break;
        }

        // 【掘墓人】【非交互职业】 每个夜晚*，你会得知今天因处决而死去的玩家的身份。数据返回格式(uint64)：
        // |  63 - 56  |  55 - 48  |  47 - 40  |  39 - 32  |  31 - 24  |  23 - 16  |  15 - 8  |    7 - 0  |（应该一回合最多两人被处决吧，1个投票和1个处女）
        // |      character2       |    pid2   |       character1      |   pid1    |   num    |  code(1)  | 
        case C_UNDERTAKER: {
            if(con1 && con3){ // 活人夜晚
                // 先确认下今天几个人被处刑了。
                uint64_t deathNum = 0;
                uint64_t deathList[2] = { 0 };
                // Although it is indeed possible that the number of deaths in a round is greater than two, 
                // but I am too lazy to care about that situation, just ignore it if the number is too large.
                for(int i = 0;i < playerNum;i++){
                    if(deathNum > 2){
                        break;
                    }
                    if(getPlayerByPID(i)->deathDate == (gameStatus >> 1)){
                        deathList[deathNum] = i; 
                        deathNum += 1;
                    }
                }
                if(con4 || con5){
                    if(deathNum == 0){
                        ret = 1;
                    } else if (deathNum == 1) {
                        uint64_t i = rand()%(13+4+4+1);
                        if(i == 21){ i=401; }
                        else if(i > 16){ i += 284; }
                        else if(i > 12){ i += 188; }
                        else {i += 101;}
                        ret = (i << 24) + (deathList[0] << 16) + (1 << 8) + 1;
                    } else if (deathNum == 2){
                        uint64_t i = rand()%(13+4+4+1);
                        if(i == 21){ i=401; }
                        else if(i > 16){ i += 284; }
                        else if(i > 12){ i += 188; }
                        else {i += 101;}
                        uint64_t j = rand()%(13+4+4+1);
                        if(j == 21){ j=401; }
                        else if(j > 16){ j += 284; }
                        else if(j > 12){ j += 188; }
                        else {j += 101;}
                        ret = (j << 48) + (deathList[1] << 40) + (i << 24) + (deathList[0] << 16) + (2 << 8) + 1;
                    }
                } else {
                    if(deathNum == 0){
                        ret = 1;
                    } else {
                        uint64_t i, j = 0;
                        if(isPlayerDrunk(deathList[0])) {
                            i = C_DRUNK;
                        } else {
                            i = getPlayerByPID(deathList[0])->characterID;
                        }
                        if(deathNum == 2) {
                            if(isPlayerDrunk(deathList[1])) {
                                j = C_DRUNK;
                            } else {
                                j = getPlayerByPID(deathList[1])->characterID;
                            }
                        }
                        ret = (j << 48) + (deathList[1] << 40) + (i << 24) + (deathList[0] << 16) + (deathNum << 8) + 1;
                    }
                }
            }
            break;
        }

        // 【僧侣】【交互职业】 每个夜晚*，选择一名玩家（除了自己）：恶魔的能力在今晚不会对那名玩家生效。数据返回格式(int64)：
        // |  63 - 56  |  55 - 48  |  47 - 40  |  39 - 32  |  31 - 24  |  23 - 16  |  15 - 8  |    7 - 0  |
        // |                                   0                                   |   pid    |  code(1)  | 
        case C_MONK: {
            if(con1 && con3){ // 活人夜晚
                int pid1 = (arg >> 8) & 0xff;
                //getPlayerByPID(pid1)->isProtected = 1;
                ret = (pid1 << 8) + 1;
            }
            break;
        }

        // 【训鸦人】【交互职业】 如果你在晚上死去，你会被唤醒并选择一名玩家：你会得知那名玩家的身份。数据返回格式(int64)：
        // |  63 - 56  |  55 - 48  |  47 - 40  |  39 - 32  |  31 - 24  |  23 - 16  |  15 - 8  |   7 - 0   |
        // |                       0                       |       character       |   pid    |  code(1)  | 
        case C_RAVENKEEPER: {
            if((!con1) && con2 && (!con3)){ // 死人一次性白天
            int pid1 = (arg >> 8) & 0xff;
                if (con4 || con5 ) {
                    int i = rand()%(13+4+4+1);
                    if(i == 21){ i=401; }
                    else if(i > 16){ i += 284; }
                    else if(i > 12){ i += 188; }
                    else {i += 101;}

                    ret = (i << 16) + (pid1 << 8) + 1;

                } else {
                    int i;
                    if(getPlayerByPID(pid1)->isDrunk) {
                        i = C_DRUNK;
                    } else {
                        i = getPlayerByPID(pid1)->characterID;
                    }

                    ret = (i << 16) + (pid1 << 8) + 1;
                }
                getPlayerByPID(pid)->isSkillUsed = 1; // todo
            }
            break;
        }

        // 【处女】【非交互职业】【纯被动职业，直接break。】当你第一次被提名时，如果提名者是村民，那么那名玩家立刻被处决。
        case C_VIRGIN: {
            break;
        }
        // 【恶魔猎手】【非交互职业】【纯被动职业，直接break。】 每局游戏限一次，在白天时，你可以公开选择一名玩家：如果那名玩家是恶魔，那么该玩家会死去。
        case C_SLAYER: {
            break;
        }
        // 【士兵】【非交互职业】【纯被动职业，直接break。】 恶魔的能力对你无效。
        case C_SOLDIER: {
            break;
        }
        // 【市长】【非交互职业】【纯被动职业，直接break。】 如果在白天只有三名玩家存活并且没有执行处决，你的阵营获胜。如果你在夜晚死去，另一位玩家可能会替代你死去。
        case C_MAYOR: {
            break;
        }
        
        // 【管家】【交互职业】 每个夜晚，选择一名玩家（除了自己）：民屯田你只能在那名玩家参与投票的处决中投票。数据返回格式(int64)：
        // |  63 - 56  |  55 - 48  |  47 - 40  |  39 - 32  |  31 - 24  |  23 - 16  |  15 - 8  |   7 - 0   |
        // |                                   0                                   |   pid    |  code(1)  | 
        case C_BUTLER: {
            if(con1 && con3){ // 活人夜晚
                // 这个角色似乎没必要对酒鬼中毒的情况特殊处理。
                //getPlayerByPID(pid)->skillArg = msgVec[msgID].arg;
                int pid1 = (arg >> 8) & 0xff;
                //getPlayerByPID(pid1)->isVotable4Bulter = 1;
                ret = (pid1 << 8) + 1;
            }
            break;
        }

        // 【酒鬼】【非交互角色】【纯被动职业，直接break。】 你喝醉了，你以为你是一个其他的村民身份，但实际上你不是。
        case C_DRUNK: {
            break;
        }
        // 【隐士】【非交互角色】【纯被动职业，直接break。】 你可能会被判定为一名邪恶玩家（爪牙或恶魔），即使你已经死亡。
        case C_RECLUSE: {
            break;
        }
        // 【圣徒】【非交互职业】【纯被动职业，直接break。】 如果你因为处决而死亡，你的阵营失败。
        case C_SAINT: {
            break;
        }
        // 【投毒者】【交互职业】 每个夜晚，选择一名玩家：那名玩家今晚和明天白天会中毒。数据返回格式(int64)：
        // |  63 - 56  |  55 - 48  |  47 - 40  |  39 - 32  |  31 - 24  |  23 - 16  |  15 - 8  |   7 - 0   |
        // |                                   0                                   |   pid    |  code(1)  | 
        case C_POISONER: {
            if(con1 && con3){ // 活人夜晚
                int pid1 = (arg >> 8) & 0xff;
                //getPlayerByPID(pid1)->isPoison = 1;
                ret = (pid1 << 8) + 1;
            }
            break;
        }
        // 【间谍】【非交互角色】【这个需要单独写，直接break。】 每个夜晚，你可以查看剧本真相。你可能会被判定为一名善良玩家（村民或外来者），即使你已经死亡。
        case C_SPY: {
            if(con1 && con3){ // 活人夜晚
                ret = 1;
            }
            break;
        }
        // 【猩红夫人】【非交互职业】【纯被动职业，直接break。】 如果有五名或更多玩家存活且恶魔死去时，你会成为恶魔。（旅行者的数量不算计在内）。若恶魔死亡，胜负判定时将该职业的角色修改为恶魔。
        case C_SCARLET_WOMAN: {
            break;
        }
        // 【男爵】【非交互职业】【纯被动职业，直接break】 场上会有额外两名外来者（外来者+2）。
        case C_BARON: {
            break;
        }
        // 【小恶魔】【交互职业】 每个夜晚*，选择一名玩家：杀死那名给玩家。如果你用这种方式自杀，一位爪牙会变成小恶魔。（其实就是刀人判定了。）数据返回格式(int64)：
        // |  63 - 56  |  55 - 48  |  47 - 40  |  39 - 32  |  31 - 24  |  23 - 16  |  15 - 8  |   7 - 0   | 
        // |           0           |   con2x   |   con1x   |    pid2   |   pid1    |   type   |  code(1)  | (con: x x x x 1 2 3 4)
        // （type: 0 - 有人被刀 | 1 - 平安夜 | 2 - 没市长，刀死 | 3 - 没市长，平安夜 | 4 - 有市长，刀死 | 5 - 有市长，平安夜）
        case C_IMP: {
            if(con1 && con3){ // 活人夜晚
                uint64_t con10 = (arg >> 8) & 0xff;
                uint64_t pid1 = (arg >> 16) & 0xff;

                uint64_t con11 = getPlayerByPID(pid1)->characterID == C_SOLDIER; // 被刀玩家是否是士兵
                uint64_t con12 = isPlayerPoisoned(pid1); // 被刀玩家是否被毒（士兵被毒可以被刀）
                uint64_t con13 = isPlayerProtectedByMonk(pid1); // 被刀玩家是否被僧侣保护
                uint64_t con14 = isPlayerPoisoned(findPlayerByCharacter(C_MONK)); // 保护玩家的僧侣是否被毒
                uint64_t con15 = getPlayerByPID(pid1)->characterID == C_MAYOR; // 被刀玩家是否是市长，如果是，就换个人死。

                if ( con10 ) {
                    // 平安夜无事发生。 
                    ret = (1 << 8) + 1;
                } else if ( ((!con11) && (!con13)) || ((!con11) && con13 && con14) || (con11 && con12 && (con14)) ){
                    // 士兵和僧侣都没有保，或者保的中毒了
                    if (con15) {
                        // 刀到市长了，换个人死。
                        int pid2;
                        while(1){
                            pid2 = rand()%playerNum;
                            if((pid2 != pid) && (!isPlayerDeath(pid2))){
                                break;
                            }
                        }
                        // 找到新人了，上面的代码重复一遍
                        uint64_t con21 = getPlayerByPID(pid2)->characterID == C_SOLDIER; // 被刀玩家是否是士兵
                        uint64_t con22 = isPlayerPoisoned(pid2); // 被刀玩家是否被毒（士兵被毒可以被刀）
                        uint64_t con23 = isPlayerProtectedByMonk(pid2); // 被刀玩家是否被僧侣保护
                        uint64_t con24 = isPlayerPoisoned(findPlayerByCharacter(C_MONK)); // 保护玩家的僧侣是否被毒

                        if ((!con21) && (!con23) || ((!con21) && con23 && (con24)) || (con21 && (!con23) && (con22))){
                            killPlayer(pid2);
                            ret = (((con21 << 3) + (con22 << 2) + (con23 << 1) + con24) << 40) + (((con11 << 3) + (con12 << 2) + (con13 << 1) + con14) << 32) + (pid2 << 24) + (pid1 << 16) + (4 << 8) + 1;
                        } else {
                            // 平安夜
                            ret = (((con21 << 3) + (con22 << 2) + (con23 << 1) + con24) << 40) + (((con11 << 3) + (con12 << 2) + (con13 << 1) + con14) << 32) + (pid2 << 24) + (pid1 << 16) + (5 << 8) + 1;
                        }
                    } else {
                        killPlayer(pid1);
                        ret = (((con11 << 3) + (con12 << 2) + (con13 << 1) + con14) << 32) + (pid1 << 16) + (2 << 8) + 1;
                    }
                } else {
                    // 平安夜
                    ret = (((con11 << 3) + (con12 << 2) + (con13 << 1) + con14) << 32) + (pid1 << 16) + (3 << 8) + 1;
                }
            }
            break;
        }
        default: 
            break;
    }

    getPlayerByPID(pid)->skillArg = ret;
    //cout<<"[Debug] 7 player "<<pid<<" Ret: "<<ret<<endl;

    return 0;
}

int Game::run(){
    
    //time_t endTime;
    endTime = time(NULL) + timeOut[0];

    printf("[Log] [Server] Room started successful!\n");


    while(1) {
        // 房间级别的循环，这个循环被 break 相当于房间被销毁。
        // 在此等待所有玩家准备完毕进入游戏。

        // clear player's isSkipStatus first
        for(int i = 0; i <= MAX_PLAYERS_PER_GAME; i++){
            if(playerArray[i] < 0){
                continue;
            }
            getPlayerByPID(i)->isSkipStatus = 0;
        }
        endTime = time(NULL) + timeOut[WAIT_GAME_BEGIN];
        int isRoomEnd = 0;
        while(1){
            int isAllPlayerPrepared = 1;
            for(int i = 0; i <= MAX_PLAYERS_PER_GAME; i++){
                if(playerArray[i] < 0){
                    continue;
                }
                if(getPlayerByPID(i)->isSkipStatus == 0){
                    isAllPlayerPrepared = 0;
                }
            }
            if(isAllPlayerPrepared){
                break;
            }
            if(time(NULL) >= endTime){
                isRoomEnd = 1;
                break;
            }
        }
        printf("[Log] [Game  ] All player prepared, start to occupy character.\n");
        for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
            sendMsg(i, "[Inf] [Game  ] All player prepared, waiting game to start...\n");
        }

        if(isRoomEnd){
            printf("[Log] [Game  ] Time out! Room end and all player exit.\n");
            break;
        }

        // 所有玩家准备完毕，开始分配职业
        AssignCharacters();
        printf("[Log] [Game  ] Characters occupied, waiting for kp...\n");
        if(isExistKP()) {
            endTime = time(NULL) + timeOut[1];
            while(1){
                if(time(NULL) >= endTime){
                    break;
                }
                if(getPlayerByPID(MAX_PLAYERS_PER_GAME)->isSkipStatus){
                    break;
                }
                if(getPlayerKP()->skillArgSP > 0){
                    getPlayerByPID(getPlayerKP()->skillArg)->characterID = getPlayerKP()->skillArgSP;
                    getPlayerKP()->skillArgSP = 0;
                }
                frameArg = 1;
                sleep(0);
            }
        }

        printf("[Log] [Game  ] KP end. Send all characters to players.\n");
        for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
            skipNP(i);
            sendMsg(i, "[Inf] [Game  ] Game started and you got the chatacter of %d!\n", getPlayerByPID(i)->characterID);
        }

        int isGameEnd = 0;

        // Occupational assignment is completed, entering the first day of the day.
        gameStatus = 1;
        while(1) {
            gameStatus += 1;
            int dayCount = gameStatus >> 1;
            int dayStatus = gameStatus & 1;

            printf("[Log] [Game  ] Day [%d]'s [%s] begun, waiting players to use their skill....\n", dayCount, dayStatus ? "Night" : "Daytime");
            for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                sendMsg(i, "[Inf] [Game  ] Day [%d]'s [%s] begun, waiting players to use their skill....\n", dayCount, dayStatus ? "Night" : "Daytime");
            }

            // player use skill.
            for(int i = 0;i <MAX_PLAYERS_PER_GAME; ++i){
                if(playerArray[i] < 0){
                    continue;
                }

                int cid = getPlayerByPID(i)->characterID;

                int con1 = !isPlayerDeath(i);                       // 玩家是否存活（1为存活）
                int con2 = (getPlayerByPID(i)->skillArg == 0);      // skillA
                int con3 = (dayStatus == 1);                        // 是否是夜晚（1为是夜晚）

                // 来个if地狱吧
                if( ((cid == C_FORTUNE_TELLER) && con1 && con3)
                || ((cid == C_MONK) && con1 && con3)
                || ((cid == C_RAVENKEEPER) && (!con1) && con2 && (!con3))
                || ((cid == C_BUTLER) && con1 && con3)
                || ((cid == C_POISONER) && con1 && con3)
                || ((cid == C_IMP) && con1 && con3) ){
                    getPlayerByPID(i)->skillArg = 2;
                    char msgstr[256];
                    sprintf(msgstr, "[Info ] [Server] Please use you skill [%lx].", getPlayerByPID(i)->skillArg);
                    gameHandler->sendSocket(playerArray[i], msgstr);
                    //sendMsg(i, newMsg(MSG_USE_SKILL, 2));
                    //cout<<"[Info ] [Server] Waitint for pid - "<<i<<" to response its skill choice."<<endl;
                } else {
                    //cout<<"[Debug] there\n";
                    // 无需交互，或技能不可用（好像也没什么要干的）
                }
            }

            // Wait for all players to return the corresponding skill parameters. 
            // When waiting, the skillarg of the player who needs to return the parameter is set to 2. 
            // If it is detected that the skillarg is not 2, the player has returned information.
            endTime = time(NULL) + timeOut[2];
            while(1){
                int isAllPlayerUsedSkill = 1;
                for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                    if(playerArray[i] < 0){
                        continue;
                    }
                    if(getPlayerByPID(i)->skillArg == 2){
                        isAllPlayerUsedSkill = 0;
                    }
                }

                if(isAllPlayerUsedSkill){
                    break;
                }
                if(time(NULL) >= endTime){
                    break;
                }

                sleep(0);
            }

            // all argument received. player use skill.
            printf("[Log] [Game  ] All players' skill argument received, start to calculate their next skill argument.\n");

            // todo drunk
            // 各职业使用技能的顺序
            // 1. 忽略不计掉几个白板职业(101.102.103.104.110.111.112. 203.204.)先把白板好人扔出去，方便后面分析
            // 2. 然后是四个开局即白板的职业(101.102.103.104)
            useSkill( findPlayerByCharacter( C_WASHERWOMAN ) );
            useSkill( findPlayerByCharacter( C_LIBRARIAN ) );
            useSkill( findPlayerByCharacter( C_INVESTIGATOR ) );
            useSkill( findPlayerByCharacter( C_CHEF ) );
            // 3. 找到投毒者并执行，后面被下毒的人当酒鬼处理
            useSkill( findPlayerByCharacter( C_POISONER ) );
            // 4. 间谍看剧本
            useSkill( findPlayerByCharacter( C_SPY ) );
            // 5. 管家选择自己的跟票对象
            useSkill( findPlayerByCharacter( C_BUTLER ) );
            // 6. 酒鬼 共情者 占卜师 掘墓人 僧侣 执行技能，但可能会因中毒被影响
            useSkill( findPlayerByCharacter( C_DRUNK ) );
            useSkill( findPlayerByCharacter( C_EMPATH ) );
            useSkill( findPlayerByCharacter( C_FORTUNE_TELLER ) );
            useSkill( findPlayerByCharacter( C_UNDERTAKER ) );
            useSkill( findPlayerByCharacter( C_MONK ) );
            // 8. 恶魔刀人(死亡判定)
            useSkill( findPlayerByCharacter( C_IMP ) );
            // 9. 市长执行技能 如果晚上死了，找人替死。发生在早上。（可能让训鸦人替死？）
            useSkill( findPlayerByCharacter( C_MAYOR ) );
            // 10. 训鸦人 若被刀，执行技能
            useSkill( findPlayerByCharacter( C_RAVENKEEPER ) );

            // wait for kp
            if(isExistKP()) {
                endTime = time(NULL) + timeOut[1];
                while(1){
                    if(time(NULL) >= endTime){
                        break;
                    }
                    if(getPlayerByPID(MAX_PLAYERS_PER_GAME)->isSkipStatus){
                        break;
                    }
                    if(getPlayerKP()->skillArgSP > 0){
                        getPlayerByPID(getPlayerKP()->skillArg)->characterID = getPlayerKP()->skillArgSP;
                        getPlayerKP()->skillArgSP = 0;
                    }
                    sleep(0);
                }
            }

            // kp is over. send data to players.
            printf("[Log] [Game  ] Skill argument calculated and kp is over. Send data to all players.\n");
            for(int i = 0;i < MAX_PLAYERS_PER_GAME; ++i){
                skipNP(i);
                sendMsg(i, "[Info ] [Server] You got the skill argument of [%lu].\n", getPlayerByPID(i)->skillArg);
            }

            // If it is night, goto the next day. If is the daytime, go to chatting time.
            if(dayStatus == 1){
                continue;
            } else {
                // player chatting. salyer can kill minion this time.
                printf("[Log] [Game  ] Start player chatting phase and the time limit is [%d] second(s). And Salyer can kill minion this time.\n", timeOut[3]);
                endTime = time(NULL) + timeOut[3];
                for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                    if(playerArray[i] < 0){
                        continue;
                    }
                    sendMsg(i, "[Info ] [Game  ] Start player chatting phase and the time limit is [%d] second(s). And Salyer can kill minion this time.\n", timeOut[3]);
                    getPlayerByPID(i)->isSkipStatus = 0;
                    getPlayerByPID(i)->skillArgSP = 0;
                }
                while(1){
                    int isAllPlayerSkipStatus = 1;
                    for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                        if(playerArray[i] < 0){
                            continue;
                        }
                        if(getPlayerByPID(i)->isSkipStatus == 0){
                            isAllPlayerSkipStatus = 0;
                        }
                    }
                    int isTimeOut = time(NULL) >= endTime;
                    
                    for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                        if(playerArray[i] < 0){
                            continue;
                        }
                        // use skillArgSP for salyer
                        if(getPlayerByPID(i)->skillArgSP > 0){
                            printf("[Info ] [Game  ] Someone trying to kill someone.\n");
                            int killpid = (getPlayerByPID(i)->skillArgSP >> 8) & 0xff;
                            int isInitiatorPoisoed = isPlayerPoisoned(i);
                            int isInitiatorSalyer = getPlayerByPID(i)->characterID == C_SLAYER;
                            int isPlayerKillable = getPlayerByPID(killpid)->characterID / 100 >= 3;

                            int isPlayerBeKilled = 0;
                            if((!isInitiatorPoisoed) && isInitiatorSalyer && (isPlayerKillable == 1)){
                                isPlayerBeKilled = 1;
                            } else {
                                isPlayerBeKilled = 0;
                            }

                            if(isExistKP()){
                                time_t lastEndTime = endTime;
                                endTime = time(NULL) + timeOut[4];
                                while(1){
                                    if(time(NULL) >= endTime){
                                        break;
                                    }
                                    if(getPlayerByPID(MAX_PLAYERS_PER_GAME)->isSkipStatus){
                                        break;
                                    }
                                    if(getPlayerKP()->skillArgSP > 0){
                                        isPlayerBeKilled = getPlayerKP()->skillArgSP;
                                        getPlayerKP()->skillArgSP = 0;
                                    }
                                    sleep(0);
                                }
                                endTime = lastEndTime + timeOut[4];
                            }

                            if(isPlayerBeKilled){
                                killPlayer(killpid);
                                char msgstr[256];
                                sprintf(msgstr, "[Info ] [Server] Player [%d] killed player [%d]!\n", i, killpid);
                                for(int i = 0;i < MAX_PLAYERS_PER_GAME; ++i){
                                    if(playerArray[i] < 0){
                                        continue;
                                    }
                                    gameHandler->sendSocket(playerArray[i], msgstr);
                                }
                            } else {
                                char msgstr[256];
                                sprintf(msgstr, "[Info ] [Server] Player [%d] trying to kill player [%d] but failed!\n", i, killpid);
                                for(int i = 0;i < MAX_PLAYERS_PER_GAME; ++i){
                                    if(playerArray[i] < 0){
                                        continue;
                                    }
                                    gameHandler->sendSocket(playerArray[i], msgstr);
                                }
                            }

                            // clear temporay argument
                            getPlayerByPID(i)->skillArgSP = 0;
                        }
                    }

                    // time out and no one trying to kill as salyer
                    if(isAllPlayerSkipStatus || isTimeOut){
                        printf("[Log] [Game  ] Time out or all player skip chatting phase.\n");
                        for(int j = 0; j < MAX_PLAYERS_PER_GAME; ++j){
                            sendMsg(j, "[Info ] [Game  ] Time out or all player skip chatting phase.\n");
                        }
                        break;
                    }

                    sleep(0);
                }

                // plater voting. wait a player to poll.
                printf("[Log] [Game  ] Start player poll phase and the time limit: [%d]. Player can poll someone now.\n", timeOut[5]);
                endTime = time(NULL) + timeOut[5];
                for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                    if(playerArray[i] < 0){
                        continue;
                    }
                    sendMsg(i, "[Log] [Game  ] Start player poll phase and the time limit: [%d]. Player can poll someone now.\n", timeOut[5]);
                    getPlayerByPID(i)->isSkipStatus = 0;
                    getPlayerByPID(i)->skillArgSP = 0;
                }
                int max_pid = -1;
                int max_num = -1;
                while(1){
                    int isAllPlayerSkipStatus = 1;
                    for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                        if(playerArray[i] < 0){
                            continue;
                        }
                        if(getPlayerByPID(i)->isSkipStatus == 0){
                            isAllPlayerSkipStatus = 0;
                        }
                    }
                    int isTimeOut = time(NULL) >= endTime;

                    int initiatorPID = -1;
                    int pollPID = -1;
                    for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                        if(playerArray[i] < 0){
                            continue;
                        }
                        if(isPlayerDeath(i)){
                            continue;
                        }
                        if((getPlayerByPID(i)->characterID == C_BUTLER) 
                        && ((getPlayerByPID(i)->skillArg >> 8) & 0xff != (getPlayerByPID(i)->skillArgSP >> 8) & 0xff)){
                            continue;
                        }
                        if(getPlayerByPID(i)->skillArgSP > 0){
                            initiatorPID = i;
                            pollPID = (getPlayerByPID(i)->skillArgSP >> 8) & 0xff;

                            break;

                        }
                    }

                    if(initiatorPID >= 0){
                        if(getPlayerByPID(pollPID)->characterID == C_VIRGIN){
                            for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                                sendMsg(i, "[Info ] [Game  ] Player [%d] trying poll player [%d], but it is virgin, player [%d] dead.\n", initiatorPID, pollPID, initiatorPID);
                            }

                            killPlayer(initiatorPID);
                            initiatorPID = -1;
                            pollPID = -1;

                            continue;
                        }

                        int vote_num = 1;

                        for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                            sendMsg(i, "[Info ] [Game  ] Player [%d] polled player [%d], all players start to vote.\n",initiatorPID, pollPID);
                        }

                        for(int i = 0; i < MAX_PLAYERS_PER_GAME*2; ++i){
                            int j = (i < MAX_PLAYERS_PER_GAME) ? i : i - MAX_PLAYERS_PER_GAME;
                            if(playerArray[j] < 0){
                                continue;
                            }
                            if(isPlayerDeath(j) && getPlayerByPID(j)->isVotedAfterDeath){
                                continue;
                            }
                            if((getPlayerByPID(j)->characterID == C_BUTLER) && ((getPlayerByPID(j)->skillArg >> 8) & 0xff != pollPID)){
                                continue;
                            }
                            if(j == initiatorPID){
                                break;
                            }

                            // waiting player [j] to vote
                            for(int k = 0; k < MAX_PLAYERS_PER_GAME; ++k){
                                sendMsg(k, "[Info ] [Game  ] Waiting for player [%d] to vote...\n", j);
                            }
                            time_t lastEndTime = endTime;
                            endTime = time(NULL) + timeOut[6];
                            while(1){
                                if(time(NULL) >= endTime){
                                    break;
                                }
                                if(getPlayerByPID(j)->skillArgSP > 0){
                                    int voted = ((getPlayerByPID(j)->skillArgSP >> 8) & 0xff);
                                    if(voted == 1){
                                        vote_num += 1;
                                    }

                                    for(int k = 0; k < MAX_PLAYERS_PER_GAME; ++k){
                                        sendMsg(k, "[Info ] [Game  ] Player [%d] voted [%s].\n", j, (voted == 1) ? "Agree" : "Objection");
                                    }

                                    break;
                                }
                                sleep(0);
                            }
                            endTime = lastEndTime + timeOut[6];
                        }

                        // all votable player voted.
                        if(vote_num > max_num){
                            max_num = vote_num;
                            max_pid = pollPID;
                            for(int k = 0; k < MAX_PLAYERS_PER_GAME; ++k){
                                sendMsg(k, "[Info ] [Game  ] All votable player voted. and player [%d] got the maxium [%d] votes for now!\n", max_pid, max_num);
                            }
                        } else {
                            for(int k = 0; k < MAX_PLAYERS_PER_GAME; ++k){
                                sendMsg(k, "[Info ] [Game  ] All votable player voted. and player [%d] got the [%d] votes but not maxium votes!\n", pollPID, vote_num);
                            }
                        }
                    }

                    // time out and no one trying to kill as salyer
                    if(isAllPlayerSkipStatus || isTimeOut){
                        printf("[Log] [Game  ] Time out or all player skip poll phase.\n");
                        for(int j = 0; j < MAX_PLAYERS_PER_GAME; ++j){
                            sendMsg(j, "[Info ] [Game  ] Time out or all player skip poll phase.\n");
                        }
                        break;
                    }

                    sleep(0);
                }

                // all player polled. kill the player with highest vote.
                // calculate the total votable player number.
                int votableNumber = 0;
                for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                    if(playerArray[i] < 0){
                        continue;
                    }
                    if(isPlayerDeath(i)){
                        if(!getPlayerByPID(i)->isVotedAfterDeath){
                            votableNumber += 1;
                            continue;
                        }
                    } else {
                       // player is alive, votaablenumber + 1;
                       votableNumber += 1; 
                    }
                }

                if(max_num * 2 > votableNumber){
                    for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                        sendMsg(i, "[Info ] [Game  ] Vote phase end. Player [%d] with the most votes [%d] will be executed.\n", max_pid, max_num);
                    }
                    killPlayer(max_pid);
                } else {
                    for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                        sendMsg(i, "[Info ] [Game  ] Vote phase end. Player [%d] with the most votes [%d] will be executed.\n", max_pid, max_num);
                    }
                }

                printf("[Inf] [Serer] Determining whether the game is over....\n");
                for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                    sendMsg(i, "[Info ] [Serer] Determining whether the game is over....\n");
                }
                // wincheck.
                // break
                int impPID = findPlayerByCharacter(C_IMP);
                if(isPlayerDeath(impPID)){
                    int scarletWomanPID = findPlayerByCharacter(C_SCARLET_WOMAN);
                    if(scarletWomanPID < 0){
                        isGameEnd = 1;
                    } else {
                        getPlayerByPID(scarletWomanPID)->characterID = C_IMP;
                    }
                }
                // if player dead in voting, re calculate the votableNumber.
                //votableNumber = 0;
                int votableGoodPerson = 0;
                int votableBadPerson = 0;
                for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                    if(playerArray[i] < 0){
                        continue;
                    }
                    if(isPlayerDeath(i)){
                        if(!getPlayerByPID(i)->isVotedAfterDeath){
                            if(getPlayerByPID(i)->characterID <= 299){
                                votableGoodPerson += 1;
                            }
                            if(getPlayerByPID(i)->characterID > 300){
                                votableBadPerson += 1;
                            }
                            continue;
                        }
                    } else {
                       // player is alive, votaablenumber + 1;
                        if(getPlayerByPID(i)->characterID <= 299){
                            votableGoodPerson += 1;
                        }
                        if(getPlayerByPID(i)->characterID > 300){
                            votableBadPerson += 1;
                        }
                    }
                }

                if((!isGameEnd) && (votableBadPerson >= votableGoodPerson)){
                    isGameEnd = 2;
                }

                if(isGameEnd > 0){
                    break;
                } else {
                    for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                        sendMsg(i, "[Info ] [Game  ] Game is not end.\n");
                    }
                }

                // nothing?
                sleep(0);
            }

            sleep(0);
        }

        // game end, go to there.
        if(isGameEnd == 1){
            for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                sendMsg(i, "[Info ] [Server] Game end! And the winner is Good person!\n");
                sendMsg(i, "[Info ] [Server] Prepare to enter the next round of the game\n");
            }
        } else if (isGameEnd == 2){
            for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
                sendMsg(i, "[Info ] [Server] Game end! And the winner is Bad person!\n");
                sendMsg(i, "[Info ] [Server] Prepare to enter the next round of the game\n");
            }
        } else {
            printf("[Error] [Server] Game end unexceptly!\n");
        }

        // clear player who quit game when game not finished.
        for(int i = 0; i < MAX_PLAYERS_PER_GAME; ++i){
            if(playerArray[i] == PID_PLAYER_QUIT_GAME){
                for(int j = i; j < MAX_PLAYERS_PER_GAME - 1; ++j){
                    playerArray[j] = playerArray[j+1];
                }
                playerArray[MAX_PLAYERS_PER_GAME] = -1;
                playerNum -= 1;
            }
        }
    }

    // there, room destory.

    return 0;
}

int Game::frame(){

    
    sleep(2);
    int endTime = 0;


    switch(gameStatus){
        case 2: {
            printf("[debug] Game is running!\n");
            uint64 oc = (frameArg >> 56) & 0xff;
            if(oc){
                endTime = time(NULL) + 60;
                // 之后在这里清空所有玩家的isSkipStatus
            }

            int timeOut = time(NULL) > endTime;
            int skipStatus = 1;
            for(int i = 0;i < MAX_PLAYERS_PER_GAME; i++){
                if(playerArray[i] < 0){
                    continue;
                }
                if(getPlayerByPID(i)->isSkipStatus == 0){
                    skipStatus = 0;
                    break;
                }
            }

            if(timeOut){
                char tmpstr[256];
                sprintf(tmpstr, "[Info ] [Server] Time out!\n");
                int tmplen = strlen(tmpstr);
                sendMsgAll(tmpstr, tmplen);
                selfDestroy();
            } else if (skipStatus) {
                printf("[] all player prepared!\n");
                gameStatus = 3;
                frameArg = ONCE_CHECK + 1;
            } else {
                frameArg = 1;
            }
            break;
        }

        // 游戏开始，给所有玩家分配职业
        case 3: {
            uint64 oc = (frameArg >> 56) & 0xff;
            if(oc){
                char msgstr[256];
                sprintf(msgstr, "[Info ] [Server] All player prepared, start to occupying characters.\n");
                //gameHandler->sendall(msgstr);
                for(int i = 0;i < playerNum; ++i){
                    gameHandler->sendSocket(playerArray[i], msgstr);
                }
                AssignCharacters();
                endTime = time(NULL) + 60;
            }

            // 若存在kp，向kp确认。
            if(isExistKP()){
                int isTimeOut = time(NULL) > endTime;
                int isConfirmed = getPlayerByPID(MAX_PLAYERS_PER_GAME)->isSkipStatus;

                if(isTimeOut || isConfirmed){
                    printf("[] kp confirmed!\n");
                    gameStatus = 4; // get into daytime
                    frameArg = ONCE_CHECK + 1;
                } else {
                    // 等待kp做出修改
                    if(getPlayerKP()->skillArgSP > 0){
                        getPlayerByPID(getPlayerKP()->skillArg)->characterID = getPlayerKP()->skillArgSP;
                        getPlayerKP()->skillArgSP = 0;
                    }
                    frameArg = 1;
                    sleep(0);
                }
            } else {
                gameStatus = 4; // get into daytime
                frameArg = ONCE_CHECK + 1;
            }
            break;
        }
        case 4: {
            // Tell every player what their character is.
            for(int i = 0;i < playerNum; ++i){
                char msgstr[256];
                sprintf(msgstr, "[Info ] [Server] You got the character of [%lu].\n", getPlayerByPID(i)->characterID);
                gameHandler->sendSocket(playerArray[i], msgstr);
            }
            gameStatus = 0; // get into daytime.
            frameArg = ONCE_CHECK + 1;
        }
        case 0: {
            // skill -> chatting -> voting
            for(int i = 0;i < playerNum; ++i){
                char msgstr[256];
                sprintf(msgstr, "[DEBUG] [Server] you are now in daytime!!!!!\n");
                gameHandler->sendSocket(playerArray[i], msgstr);
            }
            sleep(4);
        }
    }

    if(isDestroy) {
        return -1;
    }

    sleep(0);

    return 0;
}

int Game::selfDestroy(){
    isDestroy = 1;
    return 0;
}