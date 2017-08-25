#ifndef __ROUTELB_H__
#define __ROUTELB_H__

#include <list>
#include <vector>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <ext/hash_map>
#include "elb.pb.h"

//host info
struct HI
{
    HI(uint32_t myIp, int myPort, uint32_t initSucc): ip(myIp), port(myPort), succ(initSucc), err(0), overload(false) { }

    uint32_t ip;
    int port;
    uint32_t succ;
    uint32_t err;
    bool overload;
};

class LB
{
public:
    LB(): effectData(0), _status(ISPULLING)
    {
    }

    ~LB()
    {
        for (HostMapIt it = _hostMap.begin();it != _hostMap.end(); ++it)
        {
            HI* hi = it->second;
            delete hi;
        }
    }

    bool overload() const { return !empty() && _runningList.empty(); }

    bool empty() const { return _hostMap.empty(); }

    int getHost(elb::HostAddr* hp)
    {
        //TODO
        //hp->set_ip(inaddr.s_addr);
        //hp->set_port(port);
        return 0;
    }

    void getRoute(std::vector<HI*>& vec)
    {
        for (HostMapIt it = _hostMap.begin();it != _hostMap.end(); ++it)
        {
            HI* hi = it->second;
            vec.push_back(hi);
        }
    }

    void report(int ip, int port, int retcode)
    {
        uint64_t key = ((uint64_t)ip << 32) + port;
        if (_hostMap.find(key) == _hostMap.end())
            return ;
        HI* hp = _hostMap[key];
        //TODO
        if (retcode == 0)
        {
            hp->succ += 1;
        }
        else
        {
            hp->err += 1;
        }
    }

    enum STATUS
    {
        ISPULLING,
        ISNEW
    };

    long effectData;//used to repull

private:
    void addHost(uint32_t ip, int port)
    {
        uint64_t key = ((uint64_t)ip << 32) + port;
        if (_hostMap.find(key) != _hostMap.end())
            return ;
        HI* hi = new HI(ip, port, 10000/*TODO*/);
        if (!hi)
        {
            fprintf(stderr, "no more space to new HI\n");
            ::exit(1);
        }
        _hostMap[key] = hi;
        _runningList.push_back(hi);
    }

    void delHost(uint32_t ip, int port)
    {
        uint64_t key = ((uint64_t)ip << 32) + port;
        if (_hostMap.find(key) != _hostMap.end())
            return ;
        HI* hi = _hostMap[key];
        _hostMap.erase(key);
        if (hi->overload)
            _downList.remove(hi);
        else
            _runningList.remove(hi);
        delete hi;
    }

    typedef __gnu_cxx::hash_map<uint64_t, HI*> HostMap;
    typedef __gnu_cxx::hash_map<uint64_t, HI*>::iterator HostMapIt;

    HostMap _hostMap;
    std::list<HI*> _runningList, _downList;
    STATUS _status;
};

class RouteLB
{
public:
    int getHost(int modid, int cmdid, elb::GetHostRsp& rsp)
    {
        //TODO: repull
        uint64_t key = ((uint64_t)modid << 32) + cmdid;
        if (_routeMap.find(key) != _routeMap.end())
        {
            LB* lb = _routeMap[key];
            elb::HostAddr* hp = rsp.mutable_host();
            int ret = lb->getHost(hp);
            rsp.set_retcode(ret);
        }
        else
        {
            _routeMap[key] = new LB();
            rsp.set_retcode(-111111/*TODO*/);
            return -10000;//TODO
        }
        return 0;
    }

    void report(elb::ReportReq& req)
    {
        int modid = req.modid();
        int cmdid = req.cmdid();
        int retcode = req.retcode();
        int ip = req.host().ip();
        int port = req.host().port();
        uint64_t key = ((uint64_t)modid << 32) + cmdid;
        if (_routeMap.find(key) != _routeMap.end())
        {
            LB* lb = _routeMap[key];
            lb->report(ip, port, retcode);
        }
    }

    void getRoute(int modid, int cmdid, elb::GetRouteByAgentRsp& rsp)
    {
        //TODO: repull
        uint64_t key = ((uint64_t)modid << 32) + cmdid;
        if (_routeMap.find(key) != _routeMap.end())
        {
            LB* lb = _routeMap[key];
            std::vector<HI*> vec;
            lb->getRoute(vec);
            for (std::vector<HI*>::iterator it = vec.begin();it != vec.end(); ++it)
            {
                elb::HostAddr host;
                host.set_ip((*it)->ip);
                host.set_port((*it)->port);
                rsp.add_hosts()->CopyFrom(host);
            }
        }
    }

    void update(int modid, int cmdid, elb::GetRouteByAgentRsp& rsp)
    {
        
    }

private:
    typedef __gnu_cxx::hash_map<uint64_t, LB*> RouteMap;

    RouteMap _routeMap;
};

#endif
