#include <set>
#include <time.h>
#include <stdio.h>
#include <netdb.h>
#include <assert.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "Server.h"
#include "RouteLb.h"
#include "easy_reactor.h"

struct LbConfigure
{
    int initSuccCnt;   //初始的成功个数，防止刚启动时少量失败就认为过载
    int continSuccLim; //当overload节点连续成功次数超过此值，认为成功恢复
    int continErrLim;  //当正常节点连续失败次数超过此值，认为overload
    int probeNum;      //经过几次获取节点请求后，试探选择一次overload节点
    long updateTimo;   //对于每个modid/cmdid，多久更新一下本地路由,s
    long clearTimo;    //对于某个modid/cmdid下的某个host，多久清理一下负载信息
    long reportTimo;   //对于每个modid/cmdid，多久上报给reporter一次
    long ovldWaitLim;  //对于某个modid/cmdid下的某个host被判断过载后，在过载队列等待的最大时间,s
    float succRate;    //当overload节点成功率高于此值，节点变idle
    float errRate;     //当idle节点失败率高于此值，节点变overload
} LbConfig;

static uint32_t MyIp = 0;

static pthread_once_t onceLoad = PTHREAD_ONCE_INIT;

static void initLbEnviro()
{
    //load configures
    LbConfig.initSuccCnt   = config_reader::ins()->GetNumber("lb", "init_succ_cnt", 270);
    LbConfig.continSuccLim = config_reader::ins()->GetNumber("lb", "contin_succ_lim", 30);
    LbConfig.continErrLim  = config_reader::ins()->GetNumber("lb", "contin_err_lim", 30);
    LbConfig.probeNum      = config_reader::ins()->GetNumber("lb", "probe_num", 10);
    LbConfig.updateTimo    = config_reader::ins()->GetNumber("lb", "update_timeout", 15);
    LbConfig.clearTimo     = config_reader::ins()->GetNumber("lb", "clear_timeout", 15);
    LbConfig.reportTimo    = config_reader::ins()->GetNumber("lb", "report_timeout", 15);
    LbConfig.ovldWaitLim   = config_reader::ins()->GetNumber("lb", "overload_wait_lim", 300);
    LbConfig.succRate      = config_reader::ins()->GetFloat("lb", "succ_rate", 0.95);
    LbConfig.errRate       = config_reader::ins()->GetFloat("lb", "err_rate", 0.1);

    //get local IP
    char myhostname[1024];
    if (::gethostname(myhostname, 1024) == 0)
    {
        struct hostent* hd = ::gethostbyname(myhostname);
        if (hd)
        {
            struct sockaddr_in myaddr;
            myaddr.sin_addr = *(struct in_addr* )hd->h_addr;
            MyIp = ntohl(myaddr.sin_addr.s_addr);
        }
    }
    if (!MyIp)
    {
        struct in_addr inaddr;
        ::inet_aton("127.0.0.1", &inaddr);
        MyIp = ntohl(inaddr.s_addr);
    }
}

LB::~LB()
{
    for (HostMapIt it = _hostMap.begin();it != _hostMap.end(); ++it)
    {
        HI* hi = it->second;
        delete hi;
    }
}

int LB::getHost(elb::HostAddr* hp)
{
    if (_runningList.empty())//此[modid, cmdid]已经过载了，即所有节点都已经过载
    {
        //访问次数超过了probeNum，于是决定试探一次overload节点
        if (_accessCnt > LbConfig.probeNum)
        {
            _accessCnt = 0;
            //选择一个overload节点
            HI* hi = _downList.front();
            hp->set_ip(hi->ip);
            hp->set_port(hi->port);
            _downList.pop_front();
            _downList.push_back(hi);
        }
        else
        {
            //明确告诉API：已经过载了
            ++_accessCnt;
            return OVERLOAD;
        }
    }
    else
    {
        if (_downList.empty())//此[modid, cmdid]完全正常
        {
            _accessCnt = 0;//重置访问次数，仅在有节点过载时才记录
            //选择一个idle节点
            HI* hi = _runningList.front();
            hp->set_ip(hi->ip);
            hp->set_port(hi->port);
            _runningList.pop_front();
            _runningList.push_back(hi);
        }
        else//有部分节点过载了
        {
            //访问次数超过了probeNum，于是决定试探一次overload节点
            if (_accessCnt > LbConfig.probeNum)
            {
                _accessCnt = 0;
                //选择一个overload节点
                HI* hi = _downList.front();
                hp->set_ip(hi->ip);
                hp->set_port(hi->port);
                _downList.pop_front();
                _downList.push_back(hi);
            }
            else
            {
                ++_accessCnt;
                //选择一个idle节点
                HI* hi = _runningList.front();
                hp->set_ip(hi->ip);
                hp->set_port(hi->port);
                _runningList.pop_front();
                _runningList.push_back(hi);
            }
        }
    }
    return SUCCESS;
}

void LB::getRoute(std::vector<HI*>& vec)
{
    for (HostMapIt it = _hostMap.begin();it != _hostMap.end(); ++it)
    {
        HI* hi = it->second;
        vec.push_back(hi);
    }
}

void LB::persist(FILE* fp)
{
    for (HostMapIt it = _hostMap.begin();it != _hostMap.end(); ++it)
    {
        HI* hi = it->second;
        fprintf(fp, "%d %d %d %d\n", _modid, _cmdid, hi->ip, hi->port);
    }
}

void LB::report(int ip, int port, int retcode)
{
    uint64_t key = ((uint64_t)ip << 32) + port;
    if (_hostMap.find(key) == _hostMap.end())
        return ;
    HI* hi = _hostMap[key];
    //TODO
    if (retcode == 0)
    {
        hi->succ += 1;
    }
    else
    {
        hi->err += 1;
    }
}

void LB::update(elb::GetRouteRsp& rsp)
{
    assert(rsp.hosts_size() != 0);
    bool updated = false;
    //hosts who need to delete
    std::set<uint64_t> remote;
    std::set<uint64_t> todel;
    for (int i = 0;i < rsp.hosts_size(); ++i)
    {
        const elb::HostAddr& h = rsp.hosts(i);
        uint64_t key = ((uint64_t)h.ip() << 32) + h.port();
        remote.insert(key);

        if (_hostMap.find(key) == _hostMap.end())
        {
            updated = true;
            //it is new
            HI* hi = new HI(h.ip(), h.port(), LbConfig.initSuccCnt);
            if (!hi)
            {
                fprintf(stderr, "no more space to new HI\n");
                ::exit(1);
            }
            _hostMap[key] = hi;
            //add to running list
            _runningList.push_back(hi);
        }
    }
    for (HostMapIt it = _hostMap.begin();it != _hostMap.end(); ++it)
        if (remote.find(it->first) == remote.end())
            todel.insert(it->first);
    if (!todel.empty())
        updated = true;
    //delete old
    for (std::set<uint64_t>::iterator it = todel.begin();it != todel.end(); ++it)
    {
        uint64_t key = *it;
        HI* hi = _hostMap[key];

        if (hi->overload)
            _downList.remove(hi);
        else
            _runningList.remove(hi);
        _hostMap.erase(*it);
        delete hi;
    }
    //reset effectData,表明路由更新时间\有效期开始
    effectData = time(NULL);
    status = ISNEW;
}

void LB::pull()
{
    elb::GetRouteReq pullReq;
    pullReq.set_modid(_modid);
    pullReq.set_cmdid(_cmdid);
    pullQueue->send_msg(pullReq);
    //标记:路由正在拉取
    status = LB::ISPULLING;
}

void LB::report2Rpter()
{
    if (empty())
        return ;
    long currenTs = time(NULL);
    if (currenTs - lstRptTime < LbConfig.reportTimo)
        return ;
    lstRptTime = currenTs;

    elb::ReportStatusReq req;
    req.set_modid(_modid);
    req.set_cmdid(_cmdid);
    req.set_ts(time(NULL));
    req.set_caller(MyIp);

    for (ListIt it = _runningList.begin();it != _runningList.end(); ++it)
    {
        HI* hi = *it;
        elb::HostCallResult callRes;
        callRes.set_ip(hi->ip);
        callRes.set_port(hi->port);
        callRes.set_succ(hi->succ);
        callRes.set_err(hi->err);
        callRes.set_overload(false);
        req.add_results()->CopyFrom(callRes);
    }
    for (ListIt it = _downList.begin();it != _downList.end(); ++it)
    {
        HI* hi = *it;
        elb::HostCallResult callRes;
        callRes.set_ip(hi->ip);
        callRes.set_port(hi->port);
        callRes.set_succ(hi->succ);
        callRes.set_err(hi->err);
        callRes.set_overload(true);
        req.add_results()->CopyFrom(callRes);
    }

    reptQueue->send_msg(req);
}

RouteLB::RouteLB()
{
    ::pthread_once(&onceLoad, initLbEnviro);
    ::pthread_mutex_init(&_mutex, NULL);
}

int RouteLB::getHost(int modid, int cmdid, elb::GetHostRsp& rsp)
{
    uint64_t key = ((uint64_t)modid << 32) + cmdid;
    ::pthread_mutex_lock(&_mutex);
    if (_routeMap.find(key) != _routeMap.end())
    {
        LB* lb = _routeMap[key];
        elb::HostAddr* hp = rsp.mutable_host();
        int ret = lb->getHost(hp);
        rsp.set_retcode(ret);

        //检查是否需要重拉路由
        //若路由并没有正在拉取，且有效期至今已超时，则重拉取
        if (lb->status == LB::ISNEW && time(NULL) - lb->effectData > LbConfig.updateTimo)
        {
            lb->pull();
        }
        ::pthread_mutex_unlock(&_mutex);
    }
    else
    {
        LB* lb = new LB(modid, cmdid);
        if (!lb)
        {
            fprintf(stderr, "no more space to new LB\n");
            ::exit(1);
        }
        _routeMap[key] = lb;
        //拉取一下路由
        lb->pull();
        ::pthread_mutex_lock(&_mutex);
        rsp.set_retcode(NOEXIST);
        return NOEXIST;
    }
    return 0;
}

void RouteLB::report(elb::ReportReq& req)
{
    int modid = req.modid();
    int cmdid = req.cmdid();
    int retcode = req.retcode();
    int ip = req.host().ip();
    int port = req.host().port();
    uint64_t key = ((uint64_t)modid << 32) + cmdid;
    ::pthread_mutex_lock(&_mutex);
    if (_routeMap.find(key) != _routeMap.end())
    {
        LB* lb = _routeMap[key];
        lb->report(ip, port, retcode);
        //try to report to reporter
        lb->report2Rpter();
    }
    ::pthread_mutex_unlock(&_mutex);
}

void RouteLB::getRoute(int modid, int cmdid, elb::GetRouteRsp& rsp)
{
    uint64_t key = ((uint64_t)modid << 32) + cmdid;
    ::pthread_mutex_lock(&_mutex);
    if (_routeMap.find(key) != _routeMap.end())
    {
        LB* lb = _routeMap[key];
        std::vector<HI*> vec;
        lb->getRoute(vec);

        //检查是否需要重拉路由
        //若路由并没有正在拉取，且有效期至今已超时，则重拉取
        if (lb->status == LB::ISNEW && time(NULL) - lb->effectData > LbConfig.updateTimo)
        {
            lb->pull();
        }
        ::pthread_mutex_lock(&_mutex);

        for (std::vector<HI*>::iterator it = vec.begin();it != vec.end(); ++it)
        {
            elb::HostAddr host;
            host.set_ip((*it)->ip);
            host.set_port((*it)->port);
            rsp.add_hosts()->CopyFrom(host);
        }
    }
    else
    {
        LB* lb = new LB(modid, cmdid);
        if (!lb)
        {
            fprintf(stderr, "no more space to new LB\n");
            ::exit(1);
        }
        _routeMap[key] = lb;
        //拉取一下路由
        lb->pull();
        ::pthread_mutex_lock(&_mutex);
    }
}

void RouteLB::update(int modid, int cmdid, elb::GetRouteRsp& rsp)
{
    uint64_t key = ((uint64_t)modid << 32) + cmdid;
    ::pthread_mutex_lock(&_mutex);
    if (_routeMap.find(key) != _routeMap.end())
    {
        LB* lb = _routeMap[key];
        if (rsp.hosts_size() == 0)
        {
            //delete this[modid,cmdid]
            delete lb;
            _routeMap.erase(key);            
        }
        else
        {
            lb->update(rsp);
        }
    }
    ::pthread_mutex_unlock(&_mutex);
}

void RouteLB::clearPulling()
{
    ::pthread_mutex_lock(&_mutex);
    for (RouteMapIt it = _routeMap.begin();
        it != _routeMap.end(); ++it)
    {
        LB* lb = it->second;
        if (lb->status == LB::ISPULLING)
            lb->status = LB::ISNEW;
    }
    ::pthread_mutex_unlock(&_mutex);
}

void RouteLB::persistRoute()
{
    FILE* fp = NULL;
    switch (_id)
    {
        case 1:
            fp = fopen("/tmp/backupRoute.dat.1", "w");
            break;
        case 2:
            fp = fopen("/tmp/backupRoute.dat.2", "w");
            break;
        case 3:
            fp = fopen("/tmp/backupRoute.dat.3", "w");
    }
    if (fp)
    {
        ::pthread_mutex_lock(&_mutex);
        for (RouteMapIt it = _routeMap.begin();
            it != _routeMap.end(); ++it)
        {
            LB* lb = it->second;
            lb->persist(fp);
        }
        ::pthread_mutex_unlock(&_mutex);
        fclose(fp);
    }
}