#ifndef __STATICROUTE_H__
#define __STATICROUTE_H__

#include <string>
#include <vector>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ext/hash_map>

class StaticRoute
{
public:
    StaticRoute()
    {
        srand(time(NULL));
    }
    int getHost(int modid, int cmdid, std::string& ip, int& port)
    {
        if (_route.empty())
        {
            FILE* fp = fopen("/tmp/backupRoute.dat.1", "r");
            if (fp)
            {
                int imodid, icmdid, iip, iport;
                while (fscanf(fp, "%d %d %d %d", &imodid, &icmdid, &iip, &iport) != EOF)
                {
                    uint64_t key = ((uint64_t)imodid << 32) + icmdid;
                    struct in_addr saddr;
                    saddr.s_addr = iip;
                    std::string sip = inet_ntoa(saddr);
                    hostType host(sip, iport);
                    _route[key].push_back(host);
                }
                fclose(fp);
            }
            fp = fopen("/tmp/backupRoute.dat.2", "r");
            if (fp)
            {
                int imodid, icmdid, iip, iport;
                while (fscanf(fp, "%d %d %d %d", &imodid, &icmdid, &iip, &iport) != EOF)
                {
                    uint64_t key = ((uint64_t)imodid << 32) + icmdid;
                    struct in_addr saddr;
                    saddr.s_addr = iip;
                    std::string sip = inet_ntoa(saddr);
                    hostType host(sip, iport);
                    _route[key].push_back(host);
                }
                fclose(fp);
            }
            fp = fopen("/tmp/backupRoute.dat.3", "r");
            if (fp)
            {
                int imodid, icmdid, iip, iport;
                while (fscanf(fp, "%d %d %d %d", &imodid, &icmdid, &iip, &iport) != EOF)
                {
                    uint64_t key = ((uint64_t)imodid << 32) + icmdid;
                    struct in_addr saddr;
                    saddr.s_addr = iip;
                    std::string sip = inet_ntoa(saddr);
                    hostType host(sip, iport);
                    _route[key].push_back(host);
                }
                fclose(fp);
            }
        }
        uint64_t key = ((uint64_t)modid << 32) + cmdid;
        if (_route.find(key) == _route.end())
            return -1;
        int index = rand() % _route[key].size();
        hostType& host = _route[key].at(index);
        ip = host.first;
        port = host.second;
        return 0;
    }

    void freeData() { _route.clear(); }

private:
    typedef std::pair<std::string, int> hostType;
    __gnu_cxx::hash_map<uint64_t, std::vector<hostType> > _route;
};

#endif
