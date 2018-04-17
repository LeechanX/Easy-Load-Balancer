#ifndef __ROUTELB_H__
#define __ROUTELB_H__

#include <list>
#include <vector>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <ext/hash_map>
#include "elb.pb.h"

//host info
struct HI
{
    HI(uint32_t myIp, int myPort, uint32_t initSucc): 
        ip(myIp),
        port(myPort),
        succ(initSucc),
        err(0),
        rSucc(0),
        rErr(0),
        continSucc(0),
        continErr(0),
        overload(false),
        overloadTs(0),
        windErrCnt(0) {
            windowTs = time(NULL);
        }

    bool checkWindow();
    void resetIdle(uint32_t initSucc);
    void setOverload(uint32_t overloadErr);

    uint32_t ip;
    int port;
    uint32_t succ;//虚拟成功个数,用于过载、空闲判定
    uint32_t err;//虚拟失败个数,用于过载、空闲判定
    uint32_t rSucc;//真实成功个数,用于上报给reporter观察,每个modid/cmdid的上报周期重置一次
    uint32_t rErr;//真实失败个数,用于上报给reporter观察,每个modid/cmdid的上报周期重置一次
    uint32_t continSucc;
    uint32_t continErr;
    bool overload;
    long windowTs;
    long overloadTs;

    uint32_t windErrCnt;//失败率>=windErrRate的连续idle窗口个数，含此时的idle窗口
};

class LB
{
public:
    LB(int modid, int cmdid): 
        effectData(0),
        lstRptTime(0),
        status(ISPULLING),
        _modid(modid),
        _cmdid(cmdid),
        _accessCnt(0) { }

    ~LB();

    bool empty() const { return _hostMap.empty(); }

    int getHost(elb::GetHostRsp& rsp);

    void getRoute(std::vector<HI*>& vec);

    void report(int ip, int port, int retcode);
    void reportSomeSucc(int ip, int port, unsigned succCnt);
    void reportSomeErr(int ip, int port, unsigned errCnt);

    void update(elb::GetRouteRsp& rsp);

    void pull();

    void persist(FILE* fp);

    void report2Rpter();

    bool hasOvHost() const { return !_downList.empty(); }

    enum STATUS
    {
        ISPULLING,
        ISNEW
    };

    long effectData;//used to repull, last pull timestamp
    long lstRptTime;//last report timestamp
    STATUS status;
    long version;//route version

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
    RouteLB(int id);

    int getHost(int modid, int cmdid, elb::GetHostRsp& rsp);

    void report(elb::ReportReq& req);
    void batchReport(elb::CacheBatchRptReq& req);

    void getRoute(int modid, int cmdid, elb::GetRouteRsp& rsp);
    void cacheGetRoute(int modid, int cmdid, long version, elb::CacheGetRouteRsp& rsp);

    void update(int modid, int cmdid, elb::GetRouteRsp& rsp);

    //清除任何标记为：正在拉取 的[modid,cmdid]状态，当dss client网络断开后需要调用之
    void clearPulling();

    void persistRoute();

private:
    typedef __gnu_cxx::hash_map<uint64_t, LB*> RouteMap;
    typedef __gnu_cxx::hash_map<uint64_t, LB*>::iterator RouteMapIt;
    RouteMap _routeMap;
    //用于同步agent线程和后台dnsserver-client线程
    pthread_mutex_t _mutex;
    //标识自己是第几个RouteLB
    int _id;
};

#endif
