#ifndef __ELBAPI_H__
#define __ELBAPI_H__

#include <string>
#include <stdint.h>
#include "StaticRoute.h"

class elbClient
{
public:
    elbClient();

    int apiGetHost(int modid, int cmdid, int timo, std::string& ip, int& port);

    void apiReportRes(int modid, int cmdid, const std::string& ip, int port, int retcode);

private:
    int _sockfd[3];
    uint32_t _seqid;
    void* _hb;
    StaticRoute _staticRoute;
};

#endif
