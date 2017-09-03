#ifndef __ROUTELB_H__
#define __ROUTELB_H__

#include <list>
#include <vector>
#include <stdint.h>
#include <pthread.h>
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
    LB(int modid, int cmdid): effectData(0), lstRptTime(0), status(ISPULLING), _modid(modid), _cmdid(cmdid), _accessCnt(0)
    {
    }

    ~LB();

    bool empty() const { return _hostMap.empty(); }

    int getHost(elb::HostAddr* hp);

    void getRoute(std::vector<HI*>& vec);

    void report(int ip, int port, int retcode);

    void update(elb::GetRouteRsp& rsp);

    void pull();

    void report2Rpter();

    enum STATUS
    {
        ISPULLING,
        ISNEW
    };

    long effectData;//used to repull, last pull timestamp
    long lstRptTime;//last report timestamp
    STATUS status;

private:
    typedef __gnu_cxx::hash_map<uint64_t, HI*> HostMap;
    typedef __gnu_cxx::hash_map<uint64_t, HI*>::iterator HostMapIt;

    int _modid;
    int _cmdid;
    int _accessCnt;
    HostMap _hostMap;
    std::list<HI*> _runningList, _downList;
    typedef std::list<HI*>::iterator ListIt;
};

class RouteLB
{
public:
    RouteLB();

    int getHost(int modid, int cmdid, elb::GetHostRsp& rsp);

    void report(elb::ReportReq& req);

    void getRoute(int modid, int cmdid, elb::GetRouteRsp& rsp);

    void update(int modid, int cmdid, elb::GetRouteRsp& rsp);

    //清除任何标记为：正在拉取 的[modid,cmdid]状态，当dss client网络断开后需要调用之
    void clearPulling();

private:
    typedef __gnu_cxx::hash_map<uint64_t, LB*> RouteMap;
    typedef __gnu_cxx::hash_map<uint64_t, LB*>::iterator RouteMapIt;

    RouteMap _routeMap;

    //用于同步agent线程和后台dnsserver-client线程
    pthread_mutex_t _mutex;
};

#endif
