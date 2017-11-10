#include <stdlib.h>
#include <unistd.h>
#include "elbApi.h"
#include <iostream>

int main(int argc, char const *argv[])
{
    int modid = atoi(argv[1]);
    int cmdid = atoi(argv[2]);
    elbClient client;
    std::string ip;
    int port;
    int ret = client.apiRegister(modid, cmdid);//非必需使用的API
    if (ret != 0)
        std::cout << "still not exist after register" << std::endl;

    ret = client.apiGetHost(modid, cmdid, 10, ip, port);
    if (ret == 0)
    {
        std::cout << "host is " << ip << ":" << port << std::endl;
        client.apiReportRes(modid, cmdid, ip, port, 0);
    }
    else if (ret == -9998)
    {
        std::cout << modid << "," << cmdid << " not exist" << std::endl;
    }
    else
    {
        std::cout << "get error code " << ret << std::endl;
    }
    return 0;
}
