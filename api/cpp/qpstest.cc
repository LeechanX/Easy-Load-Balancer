#include <stdlib.h>
#include "elbApi.h"
#include <iostream>

struct Item
{
    int modid;
    int cmdid;
};

void* mockApi(void* args)
{
    Item* item = (Item*)args;
    int modid = item->modid, cmdid = item->cmdid;
    std::string ip;
    int port;
    int ret;
    long qps = 0;
    elbClient client;
    long lst = time(NULL);
    while (1)
    {
        ret = client.apiGetHost(modid, cmdid, 10, ip, port);
        if (ret == 0 || ret == -9998)
        {
            ++qps;
            if (ret == 0)
            {
                client.apiReportRes(modid, cmdid, ip, port, 0);
            }
        }
        else
        {
            //printf("[%d,%d] get error %d\n", modid, cmdid, ret);
        }
        long curr = time(NULL);
        if (curr - lst >= 1)
        {
            lst = curr;
            printf("[%ld]\n", qps);
            qps = 0;
        }
    }
    return NULL;
}

int main(int argc, char const *argv[])
{
    int cnt = atoi(argv[1]);
    Item* items = new Item[cnt];
    pthread_t* tids = new pthread_t[cnt];
    for (int i = 0;i < cnt; ++i)
    {
        items[i].modid = 10000 + i;
        items[i].cmdid = 1001;
    }
    for (int i = 0;i < cnt; ++i)
        pthread_create(&tids[i], NULL, mockApi, &items[i]);
    for (int i = 0;i < cnt; ++i)
        pthread_join(tids[i], NULL);
    return 0;
}