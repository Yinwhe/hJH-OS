/*
 * @Author: Yinwhe
 * @Date: 2021-07-10 20:06:58
 * @LastEditors: Yinwhe
 * @LastEditTime: 2021-07-12 12:38:18
 * @Description: file information
 * @Copyright: Copyright (c) 2021
 */
#include "sleeplock.h"
#include "spinlock.h"
#include "sched.h"
#include "put.h"
//#define DEBUG
void acquiresleep(struct sleeplock *lk)
{
    acquire(&lk->lk);
    while (lk->lock){
        sleep(lk, &lk->lk);
    }
    lk->lock = 1;
    lk->owner = getpid();
    #ifdef DEBUG
    puts(lk->name);
	puts(" acqiresleep sucessfully!\n");
    #endif
    release(&lk->lk);
}

void releasesleep(struct sleeplock *lk)
{
    acquire(&lk->lk);
    lk->lock = 0;
    lk->owner = -1;
    wakeup(lk);
    #ifdef DEBUG
    puts(lk->name);
	puts(" releasesleep sucessfully!\n");
    #endif
    release(&lk->lk);
}

int holdingsleep(struct sleeplock *lk)
{
    int r;
    acquire(&lk->lk);
    r = lk->lock && (lk->owner == getpid());
    release(&lk->lk);
    return r;
}

void initsleeplock(struct sleeplock *lk, char *name)
{
    initlock(&(lk->lk), "sleep lock");
    lk->lock=0;
    lk->name=name;
    lk->owner=-1;
}
