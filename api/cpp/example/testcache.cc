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
    int port, ret;
#if 0
    for (int i = 0;i < 1000; ++i)
    {
        ret = client.apiGetHost(modid, cmdid, 10, ip, port);
        if (ret == 0)
        {
            //std::cout << "host is " << ip << ":" << port << std::endl;
            if (rand() % 6 == 0)
                client.apiReportRes(modid, cmdid, ip, port, 0);
            else
                client.apiReportRes(modid, cmdid, ip, port, 1);
        }
        else if (ret == -9998)
        {
            //std::cout << modid << "," << cmdid << " not exist" << std::endl;
        }
        else
        {
            //std::cout << "get error code " << ret << std::endl;
        }
        usleep(10000);
    }
    int x;
    printf("continue>:");
    scanf("%d", &x);
#endif
    for (int i = 0;i < 30000; ++i)
    {
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
            //std::cout << "get error code " << ret << std::endl;
        }
        usleep(10000);
    }
    return 0;
}
