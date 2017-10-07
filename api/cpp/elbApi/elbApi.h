#ifndef __ELBAPI_H__
#define __ELBAPI_H__

#include <string>
#include <vector>
#include <stdint.h>
#include "cacheLayer.h"
#include "StaticRoute.h"

class elbClient
{
public:
    elbClient();

    ~elbClient();

    int apiGetHost(int modid, int cmdid, int timo, std::string& ip, int& port);

    void apiReportRes(int modid, int cmdid, const std::string& ip, int port, int retcode);

    int apiGetRoute(int modid, int cmdid, std::vector<std::pair<std::string, int> >& route);

    int apiRegister(int modid, int cmdid);//非必需使用的API

private:
    //向agent批量上报某mod
    void batchReportRes(CacheUnit* cacheItem);
    //向agent获取某mod的路由
    int getRoute4Cache(int modid, int cmdid, long ts);

    int _sockfd[3];
    uint32_t _seqid;
    void* _hb;
    StaticRoute _staticRoute;
    bool _agentOff;
    CacheLayer _cacheLayer;
};

#endif
