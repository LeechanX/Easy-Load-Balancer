#include "elbApi.h"
#include <stdlib.h>
#include <iostream>

int main(int argc, char const *argv[])
{
    if (argc != 3)
    {
        printf("./getroute modid cmdid\n");
        exit(1);
    }
    int modid = atoi(argv[1]);
    int cmdid = atoi(argv[2]);

    elbClient client;

    std::vector<std::pair<std::string, int> > route;
    int ret = client.apiGetRoute(modid, cmdid, route);
    if (ret == 0)
    {
        std::cout << "route for " << modid << "," << cmdid << ":" << std::endl;
        if (route.size() == 0)
            std::cout << "No route right now" << std::endl;
        for (unsigned i = 0;i < route.size(); ++i)
        {
            std::pair<std::string, int> host = route[i];
            std::cout << host.first << ":" << host.second << std::endl;
        }
    }
    return 0;
}
