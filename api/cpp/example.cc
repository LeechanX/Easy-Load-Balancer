#include "elbApi.h"
#include <iostream>

int main(int argc, char const *argv[])
{
    elbClient client;
    std::string ip;
    int port;
    int ret = client.apiGetHost(10001, 1001, 10, ip, port);
    if (ret == 0)
    {
        std::cout << "host is " << ip << ":" << port << std::endl;
        client.apiReportRes(10001, 1001, ip, port, 0);
    }
    return 0;
}