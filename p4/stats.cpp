#include "stats.h"
#include "statrunner.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "QuadraticProbing.h"
#include <stdint.h>

//struct t10;
//struct ply;
//struct tm;

typedef struct ply{ //stored in hashtable for players
    float avg;
    int hits;
    int atBats;
    struct t10 *MLB10; //entry in top10, NULL if not in top10
    struct t10 *team10; //entry in top10, NULL if not in top10
    const char *name;
    const char *team;
} ply;

typedef struct team{ //stored in hashtable for teams
    struct t10 *top10;
    int count; //# of elements in top10
    QuadraticHashTable<uintptr_t> *players;
} team;

//top 10 is a double linked list
typedef struct t10 { //stored in tm and as a global var (for MLB)
    struct t10 *bigger; //NULL if it does not exist
    struct t10 *smaller; //NULL if it does not exist
    struct ply *player;
} t10;

t10 *MLB = NULL; //points to the smallest member of top10 of all teams
int MLBcount = 0; //# of elements in top10
QuadraticHashTable<int> *teams;

#define UPDATE_AVG(PLY) \
    PLY->avg = ((float)(PLY->hits))/(PLY->atBats)

#define IS_MLB(TEAM) (*((int *)TEAM) == 0x424c4d)
#define HASH_TEAM(TEAM) ((*((int *)TEAM)) << 8) //faster than strcmp

Stats::Stats()
{
    teams = new QuadraticHashTable<int>(0, 2*30);
}

#define INIT_PLAYER(PLY, NAME, TEAM) do{\
    PLY = (ply *)malloc(sizeof(ply));\
    PLY->hits = hit;\
    PLY->atBats = 1;\
    PLY->avg = (float)hit;\
    PLY->name = NAME;\
    PLY->team = TEAM;\
} while(0)

#define PRINT_TOP10(T10) do{\
    int __i;\
    t10 *__tmp = T10;\
    for(__i = 1; __tmp != NULL; __i++, __tmp = __tmp->bigger)\
    {\
        printf("%d. %s (%s) %f\n", __i, __tmp->player->name, __tmp->player->team, __tmp->player->avg);\
    }\
    if(*count == 10 && __i != 11)\
    {\
        printf("HOLY FUCK SOMETHING BAD HAPPENED %d\n", __i);\
        exit(1);\
    }\
} while(0)

inline void INSERT_TOP10(t10 **top10Ptr, ply *player, int *count)
{
    t10 *top10 = *top10Ptr;
    bool isMLB = top10 == MLB;

    bool was10 = *count == 10;

    if(isMLB && *count == 10)
    {
        printf("ismlb and count is 10\n\n");
        PRINT_TOP10(top10);
    }

    t10 *i, *lasti = NULL;
    for(i = top10; i && player->avg > i->player->avg; i = i->bigger)
        lasti = i;
    //i is the first one bigger than player
    //lasti is the last one smaller than player

    t10 *p10 = (t10 *)malloc(sizeof(t10));
    p10->player = player;
    if(isMLB)
        player->MLB10 = p10;
    else
        player->team10 = p10;

    p10->bigger = i;
    p10->smaller = lasti;

    if(lasti) //if we're not inserting at the beginning
        lasti->bigger = p10;
    else if(*count == 10) //oh shit, we are, but its full so fuck it
        return;
    else //ok make it at the front then.
        *top10Ptr = p10;

    if(i) //if we're not inserting at the end
        i->smaller = p10;

    if(*count == 10)
    {
        top10->bigger->smaller = NULL;

        if(isMLB)
            top10->player->MLB10 = NULL;
        else
            top10->player->team10 = NULL;

        *top10Ptr = top10->bigger;

        free(top10);

    } else *count = *count + 1;

    if(isMLB && was10)
    {
        printf("ismlb and count is 11\n\n");
        PRINT_TOP10(*top10Ptr);
    }
}

inline void UPDATE_TOP10(t10 **top10Ptr, ply *player, int *count, int hit)
{
    t10 *top10 = *top10Ptr;
    bool isMLB = top10 == MLB;
    t10 *ply10 = isMLB ? player->MLB10 : player->team10;

    //if player is not in the top10, then try to insert.
    if(!ply10)
        return INSERT_TOP10(top10Ptr, player, count);

    if(hit)
    {
        while(ply10->bigger && player->avg > ply10->bigger->player->avg)
        {
            t10 *one = ply10->smaller;
            t10 *two = ply10->bigger;
            t10 *three = ply10;
            t10 *four = ply10->bigger->bigger;

            if(one) one->bigger = two;
            two->smaller = one;
            two->bigger = three;
            three->smaller = two;
            three->bigger = four;
            if(four) four->smaller = three;
        }
    }
    else
    {
        while(ply10->smaller && player->avg < ply10->smaller->player->avg)
        {
            t10 *one = ply10->smaller->smaller;
            t10 *two = ply10;
            t10 *three = ply10->smaller;
            t10 *four = ply10->bigger;

            if(one) one->bigger = two;
            two->smaller = one;
            two->bigger = three;
            three->smaller = two;
            three->bigger = four;
            if(four) four->smaller = three;
        }
    }
}

#define YEAH -1

void Stats::update(const char nameStr[25], const char teamStr[4], int hit, int operationNum)
{
    if(operationNum == YEAH) exit(1);
    printf("Operation num: %d\n", operationNum);
    //LOL FUCK USING STRCMP im gonna cheat
    char **ptr = (char **)nameStr;
    uintptr_t namePtr = (uintptr_t)ptr;

    team *theTeam = (team *)(teams->find(HASH_TEAM(teamStr)));

    if(!theTeam) //no team found, so create it
    {
        theTeam     = (team *)malloc(sizeof(tm));
        ply *player;
        t10 *top10  = (t10 *)malloc(sizeof(t10));

        //create player
        INIT_PLAYER(player, nameStr, teamStr);
        player->team10 = top10;

        //put him in his team's top10
        top10->bigger = NULL;
        top10->smaller = NULL;
        top10->player = player;

        //actually create the team
        theTeam->top10 = top10;
        theTeam->count = 1;
        theTeam->players = new QuadraticHashTable<uintptr_t>(0, 2*40);

        //insert player into team hash table
        theTeam->players->insert(namePtr, player);
        //insert into team hash table
        teams->insert(HASH_TEAM(teamStr), theTeam);

        //MLBcount++;
        printf("New team!!!!!! %s\n", teamStr);
        printf("And player!!!! %s %s\n", nameStr, hit ? "hit":"missed");

        if(!MLB)
        {

            MLB = (t10 *)malloc(sizeof(t10));
            MLB->bigger = NULL;
            MLB->smaller = NULL;
            MLB->player = player;
            player->MLB10 = MLB;
            MLBcount = 1;
        }
        else
        {
            INSERT_TOP10(&MLB, player, &MLBcount);
        }

        //INSERT_TOP10(&top10, player, &(theTeam->count));
    }
    else
    {
        ply *player = (ply *)(theTeam->players->find(namePtr));
        if(!player)
        {
            INIT_PLAYER(player, nameStr, teamStr);
            theTeam->players->insert(namePtr, player);
            printf("New player!!!! %s (%s) %s\n", nameStr, teamStr, hit?"hit":"missed");

            INSERT_TOP10(&(theTeam->top10), player, &(theTeam->count));
            INSERT_TOP10(&MLB, player, &MLBcount);
        }
        else
        {
            player->hits += hit;
            player->atBats++;
            UPDATE_AVG(player);
            printf("Updating average for %s: %d/%d = %f\n", nameStr, player->hits, player->atBats, player->avg);
            UPDATE_TOP10(&(theTeam->top10), player, &(theTeam->count), hit);
            UPDATE_TOP10(&MLB, player, &MLBcount, hit);
        }

    }
}

void Stats::query(const char teamStr[4], Player top10Players[10], int operationNum)
{
    if(operationNum == YEAH) exit(1);
    return;
    printf("\nvv FOR %d (%s) vv MLBcount = %d\n", operationNum, teamStr, MLBcount);
    t10 *top10;
    if(IS_MLB(teamStr))
    {
        top10 = MLB;
    }
    else
    {
        top10 = (t10 *)(teams->find(HASH_TEAM(teamStr)));
    }

    int i;
    for(i = 0; top10 && i < 10; i++, top10 = top10->bigger)
    {
        ply *p = top10->player;
        printf("%p %p\n", p->name, p->team);
        printf("%d. %s (%s) %d/%d = %f\n", i, p->name, p->team, p->hits, p->atBats, p->avg);
        //strcpy(top10Players[i].name, top10->player->name);
        //strcpy(top10Players[i].team, top10->player->team);
        //top10Players[i].name = top10->player->name;
        //top10Players[i].team = top10->player->team;
    }
}

