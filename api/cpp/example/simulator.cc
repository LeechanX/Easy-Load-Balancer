#include <stdlib.h>
#include <unistd.h>
#include "elbApi.h"
#include <iostream>
#include <time.h>
#include <map>

int main(int argc, char const *argv[])
{
    int modid = atoi(argv[1]);
    int cmdid = atoi(argv[2]);
    elbClient client;
    std::string ip;
    std::map<std::string, std::pair<int, int> > stat;
    int port;
    int ret = client.apiRegister(modid, cmdid);//非必需使用的API
    if (ret != 0)
        std::cout << "still not exist after register" << std::endl;
    
    srand(time(NULL));
    for (int i = 0;i < 90; ++i) {
        ret = client.apiGetHost(modid, cmdid, 10, ip, port);
        if (ret == 0)
        {
            if (stat.find(ip) == stat.end()) {
                std::pair<int, int> pair(0, 0);
                stat[ip] = pair;
            }
            std::cout << "host is " << ip << ":" << port << " ";
            if (rand() % 10 < 2)//以80%的概率产生调用失败
            {
                if (stat.find(ip) == stat.end()) {
                    std::pair<int, int> pair(0, 0);
                    stat[ip] = pair;
                }
                stat[ip].second += 1;
                client.apiReportRes(modid, cmdid, ip, port, 1);
                std::cout << "call error" << std::endl;
            }
            else
            {
                stat[ip].first += 1;
                client.apiReportRes(modid, cmdid, ip, port, 0);
                std::cout << "call success" << std::endl;
            }
        }
        else if (ret == -9998)
        {
            std::cout << modid << "," << cmdid << " not exist" << std::endl;
        }
        else
        {
            std::cout << "get error code " << ret << std::endl;
        }
        //::usleep(600000);
        ::usleep(6000);
    }

    std::map<std::string, std::pair<int, int> >::iterator it;
    for (it = stat.begin();it != stat.end(); ++it)
    {
        std::cout << "ip: " << it->first << ": ";
        std::cout << "success: " << it->second.first << "; ";
        std::cout << "error: " << it->second.second << std::endl;
    }
    return 0;
}
