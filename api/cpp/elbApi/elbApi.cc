#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "easy_reactor.h"
#include "elbApi.h"
#include "elb.pb.h"
#include "HeartBeat.h"

elbClient::elbClient(): _seqid(0)
{
    struct sockaddr_in servaddr;
    ::bzero(&servaddr, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    ::inet_aton("127.0.0.1", &servaddr.sin_addr);

    for (int i = 0;i < 3; ++i)
    {
        _sockfd[i] = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
        if (_sockfd[i] == -1)
        {
            perror("socket()");
            ::exit(1);
        }
        servaddr.sin_port = htons(8888 + i);
        int ret = ::connect(_sockfd[i], (const struct sockaddr*)&servaddr, sizeof servaddr);
        if (ret == -1)
        {
            perror("connect()");
            ::exit(1);
        }
    }
    _hb = new HeartBeat();
    if (!_hb)
    {
        fprintf(stderr, "no space to new HeartBeat\n");
        ::exit(1);
    }
}

elbClient::~elbClient()
{
    vector<CacheUnit*> all;
    vector<CacheUnit*>::iterator it;
    _cacheLayer.getAll(all);
    for (it = all.begin();it != all.end(); ++it)
    {
        CacheUnit* cu = *it;
        batchReportRes(cu);
    }
    for (int i = 0;i < 3; ++i)
        ::close(_sockfd[i]);
    delete (HeartBeat*)_hb;
}

int elbClient::apiGetHost(int modid, int cmdid, int timo, std::string& ip, int& port)
{
    if (((HeartBeat*)_hb)->die())
    {
        _agentOff = true;
        int ret = _staticRoute.getHost(modid, cmdid, ip, port);
        if (ret == -1)
        {
            fprintf(stderr, "[%d,%d] is not exist!\n", modid, cmdid);
            return -9998;// = lbagent's NOEXIST
        }
        return 0;
    }
    _agentOff = false;
    _staticRoute.freeData();
    //get host from cache
    long currTs = time(NULL);
    CacheUnit* cacheItem = _cacheLayer.getCache(modid, cmdid);

    //此mod不在缓存中，则向agent获取
    if (!cacheItem)
    {
        int ret = getRoute4Cache(modid, cmdid, currTs);
        if (ret) return ret;
        //从缓存中获取刚拉到的此mod条目
        cacheItem = _cacheLayer.getCache(modid, cmdid);
    }
    if (!cacheItem)
    {
        //说明agent端不存在此mod，返回不存在错误
        return -9998;
    }
    //检查此mod的缓存条目是否超时：
    //如果上次更新是2s前，则重拉取一次
    //顺便先把暂存的上报信息报上去（如果有的话）
    if (currTs - cacheItem->lstUpdTs >= 2)
    {
        batchReportRes(cacheItem);
        getRoute4Cache(modid, cmdid, currTs);
        //从缓存中获取刚拉到的此mod条目
        cacheItem = _cacheLayer.getCache(modid, cmdid);
    }
    if (!cacheItem)
    {
        //说明agent端已经不存在此mod了，返回不存在错误
        return -9998;
    }

    //如果此mod在API端没有overload节点，则从缓存中获取节点，否则走网络
    if (!cacheItem->overload)
    {
        cacheItem->getHost(ip, port);
        return 0;
    }

    //get host from local network
    uint32_t seq = _seqid++;
    elb::GetHostReq req;
    req.set_seq(seq);
    req.set_modid(modid);
    req.set_cmdid(cmdid);
    //send
    char wbuf[4096], rbuf[81920];
    commu_head head;
    head.length = req.ByteSize();
    head.cmdid = elb::GetHostReqId;
    ::memcpy(wbuf, &head, COMMU_HEAD_LENGTH);
    req.SerializeToArray(wbuf + COMMU_HEAD_LENGTH, head.length);
    int index = (modid + cmdid) % 3;
    int ret = ::sendto(_sockfd[index], wbuf, head.length + COMMU_HEAD_LENGTH , 0, NULL, 0);
    if (ret == -1)
    {
        perror("sendto");
        return -9999;
    }

    struct timeval tv;
    tv.tv_sec = timo / 1000;
    tv.tv_usec = (timo % 1000) * 1000;
    ::setsockopt(_sockfd[index], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int pkgLen = ::recvfrom(_sockfd[index], rbuf, sizeof rbuf, 0, NULL, NULL);
    if (pkgLen == -1)
    {
        perror("recvfrom");
        return -9999;
    }

    ::memcpy(&head, rbuf, COMMU_HEAD_LENGTH);
    elb::GetHostRsp rsp;
    if (head.cmdid != elb::GetHostRspId || 
        !rsp.ParseFromArray(rbuf + COMMU_HEAD_LENGTH, pkgLen - COMMU_HEAD_LENGTH))
    {
        fprintf(stderr, "package format error: head.length is %d, pkgLen is %d, head.cmdid is %d, target cmdid is %d\n",
            head.length, pkgLen, head.cmdid, elb::GetHostRspId);
        return -9999;
    }
    while (rsp.seq() < seq)
    {
        //recv again
        pkgLen = ::recvfrom(_sockfd[index], rbuf, sizeof rbuf, 0, NULL, NULL);
        if (pkgLen == -1)
        {
            perror("recvfrom");
            return -9999;
        }
        ::memcpy(&head, rbuf, COMMU_HEAD_LENGTH);
        if (head.cmdid != elb::GetHostRspId || 
            !rsp.ParseFromArray(rbuf + COMMU_HEAD_LENGTH, pkgLen - COMMU_HEAD_LENGTH))
        {
            fprintf(stderr, "package format error\n");
            return -9999;
        }
    }
    if (rsp.seq() != seq || rsp.modid() != modid || rsp.cmdid() != cmdid)
    {
        fprintf(stderr, "package content error\n");
        return -9999;
    }

    if (rsp.retcode() == 0)
    {
        const elb::HostAddr host = rsp.host();
        struct in_addr inaddr;
        inaddr.s_addr = host.ip();
        ip = ::inet_ntoa(inaddr);
        port = host.port();
    }
    return rsp.retcode();
}

int elbClient::getRoute4Cache(int modid, int cmdid, long ts)
{
    CacheUnit* cacheItem = _cacheLayer.getCache(modid, cmdid);
    elb::CacheGetRouteReq req;
    req.set_modid(modid);
    req.set_cmdid(cmdid);
    ////首次获取时，version=-1
    if (cacheItem)
        req.set_version(cacheItem->version);
    else
        req.set_version(-1);
    //api向agent发起获取\更新路由请求
    char wbuf[4096], rbuf[81920];
    commu_head head;
    head.length = req.ByteSize();
    head.cmdid = elb::CacheGetRouteReqId;
    ::memcpy(wbuf, &head, COMMU_HEAD_LENGTH);
    req.SerializeToArray(wbuf + COMMU_HEAD_LENGTH, head.length);
    int index = (modid + cmdid) % 3;
    int ret = ::sendto(_sockfd[index], wbuf, head.length + COMMU_HEAD_LENGTH , 0, NULL, 0);
    if (ret == -1)
    {
        perror("sendto");
        return -9999;
    }
    //获取时间为50ms
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    ::setsockopt(_sockfd[index], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    int pkgLen = ::recvfrom(_sockfd[index], rbuf, sizeof rbuf, 0, NULL, NULL);
    if (pkgLen == -1)
    {
        perror("recvfrom");
        return -9999;
    }
    ::memcpy(&head, rbuf, COMMU_HEAD_LENGTH);
    elb::CacheGetRouteRsp rsp;
    if (head.cmdid != elb::CacheGetRouteRspId ||
        !rsp.ParseFromArray(rbuf + COMMU_HEAD_LENGTH, pkgLen - COMMU_HEAD_LENGTH))
    {
        fprintf(stderr, "package format error: head.length is %d, pkgLen is %d, head.cmdid is %d, target cmdid is %d\n",
            head.length, pkgLen, head.cmdid, elb::GetHostRspId);
        return -9999;
    }
    if (rsp.modid() != modid || rsp.cmdid() != cmdid)
    {
        fprintf(stderr, "package content error\n");
        return -9999;
    }
    //已经拿到了正确的路由
    if (rsp.version() == -1)
    {
        //如果agent端此mod路由为空，则删除本地cache（如果有的话）
        _cacheLayer.rmCache(modid, cmdid);
    }
    else if (!cacheItem || rsp.version() != cacheItem->version)
    {
        //如果本地缓存不存在此mod，或者版本号与agent上此mod不同则更新本地路由
        if (!cacheItem)
        {
            cacheItem = new CacheUnit();
            if (!cacheItem)
                ::exit(1);
            //添加cacheItem到本地缓存
            cacheItem->modid = modid;
            cacheItem->cmdid = cmdid;
            _cacheLayer.addCache(modid, cmdid, cacheItem);
        }
        cacheItem->overload = rsp.overload();
        cacheItem->version = rsp.version();
        cacheItem->succCnt = 0;
        cacheItem->lstUpdTs = ts;
        cacheItem->nodeList.clear();
        cacheItem->succAccum.clear();
        //添加路由信息
        for (int i = 0;i < rsp.route_size(); ++i)
        {
            const elb::HostAddr& ha = rsp.route(i);
            pair<int, int> hd;
            hd.first = ha.ip();
            hd.second = ha.port();
            cacheItem->nodeList.push_back(hd);
            uint64_t key = ((uint64_t)hd.first << 32) + hd.second;
            cacheItem->succAccum[key] = 0;
        }
    }
    else
    {
        //版本一致
        cacheItem->overload = rsp.overload();
        cacheItem->succCnt = 0;
        cacheItem->lstUpdTs = ts;
    }
    return 0;
}

void elbClient::batchReportRes(CacheUnit* cacheItem)
{
    if (cacheItem && cacheItem->succCnt)
    {
        cacheItem->succCnt = 0;//清空调用信息
        elb::CacheBatchRptReq req;
        req.set_modid(cacheItem->modid);
        req.set_cmdid(cacheItem->cmdid);
        hash_map<uint64_t, uint32_t>::iterator it;
        for (it = cacheItem->succAccum.begin();it != cacheItem->succAccum.end(); ++it)
        {
            //如果此节点有调用信息
            if (it->second)
            {
                elb::HostBatchCallRes cr;
                cr.set_ip((int)(it->first >> 32));
                cr.set_port((int)it->first);
                cr.set_succcnt(it->second);
                req.add_results()->CopyFrom(cr);
            }
            //reset accumulator to 0
            it->second = 0;
        }
        //send to agent
        char wbuf[4096];
        commu_head head;
        head.length = req.ByteSize();
        head.cmdid = elb::CacheBatchRptReqId;
        ::memcpy(wbuf, &head, COMMU_HEAD_LENGTH);
        req.SerializeToArray(wbuf + COMMU_HEAD_LENGTH, head.length);

        int index = (cacheItem->modid + cacheItem->cmdid) % 3;
        int ret = ::sendto(_sockfd[index], wbuf, head.length + COMMU_HEAD_LENGTH , 0, NULL, 0);
        if (ret == -1)
            perror("sendto()");
    }
}

void elbClient::apiReportRes(int modid, int cmdid, const std::string& ip, int port, int retcode)
{
    if (_agentOff) return ;
    struct in_addr inaddr;
    ::inet_aton(ip.c_str(), &inaddr);
    int ipn = inaddr.s_addr;//ip number

    //调用成功，且此mod无节点过载
    CacheUnit* cacheItem = _cacheLayer.getCache(modid, cmdid);
    //此mod无节点过载
    if (cacheItem && !cacheItem->overload)
    {
        if (retcode == 0)
        {
            //上报到缓存即可
            cacheItem->report(ipn, port);
            return ;
        }
        else
        {
            //retcode != 0
            //立即将之前暂存的report状态上报予agent，如果有的话
            batchReportRes(cacheItem);//将之前暂存的此mod的report状态上报予agent
            //然后需要通过网络上报本次状态, so don't return
        }
    }
    
    //通过网络上报本次状态
    //report status by local network
    elb::ReportReq req;
    req.set_modid(modid);
    req.set_cmdid(cmdid);
    req.set_retcode(retcode);

    elb::HostAddr* hp = req.mutable_host();
    hp->set_ip(ipn);
    hp->set_port(port);

    //send
    char wbuf[4096];
    commu_head head;
    head.length = req.ByteSize();
    head.cmdid = elb::ReportReqId;
    ::memcpy(wbuf, &head, COMMU_HEAD_LENGTH);
    req.SerializeToArray(wbuf + COMMU_HEAD_LENGTH, head.length);

    int index = (modid + cmdid) % 3;
    int ret = ::sendto(_sockfd[index], wbuf, head.length + COMMU_HEAD_LENGTH , 0, NULL, 0);
    if (ret == -1)
        perror("sendto()");
}

int elbClient::apiGetRoute(int modid, int cmdid, std::vector<std::pair<std::string, int> >& route)
{
    if (((HeartBeat*)_hb)->die())
    {
        fprintf(stderr, "agent offline\n");
        return -1;
    }

    elb::GetRouteReq req;
    req.set_modid(modid);
    req.set_cmdid(cmdid);
    //send
    char wbuf[4096];
    char* rbuf = new char[40960];
    assert(rbuf);
    commu_head head;
    head.length = req.ByteSize();
    head.cmdid = elb::GetRouteByToolReqId;
    ::memcpy(wbuf, &head, COMMU_HEAD_LENGTH);
    req.SerializeToArray(wbuf + COMMU_HEAD_LENGTH, head.length);
    int index = (modid + cmdid) % 3;
    int ret = ::sendto(_sockfd[index], wbuf, head.length + COMMU_HEAD_LENGTH , 0, NULL, 0);
    if (ret == -1)
    {
        perror("sendto");
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    ::setsockopt(_sockfd[index], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int pkgLen = ::recvfrom(_sockfd[index], rbuf, 40960, 0, NULL, NULL);
    if (pkgLen == -1)
    {
        perror("recvfrom");
        return -1;
    }

    ::memcpy(&head, rbuf, COMMU_HEAD_LENGTH);
    elb::GetRouteRsp rsp;
    if (head.cmdid != elb::GetRouteByToolRspId || 
        !rsp.ParseFromArray(rbuf + COMMU_HEAD_LENGTH, pkgLen - COMMU_HEAD_LENGTH))
    {
        fprintf(stderr, "receive data format error\n");
        return -1;
    }

    for (int i = 0;i < rsp.hosts_size(); ++i)
    {
        const elb::HostAddr& host = rsp.hosts(i);
        struct in_addr inaddr;
        inaddr.s_addr = host.ip();
        std::string ip = ::inet_ntoa(inaddr);
        int port = host.port();
        route.push_back(std::pair<std::string, int>(ip, port));
    }
    delete[] rbuf;
    return 0;
}

//非必需使用的API
int elbClient::apiRegister(int modid, int cmdid)
{
    std::vector<std::pair<std::string, int> > route;
    int retryCnt = 0;
    while (route.empty() && retryCnt < 3)
    {
        apiGetRoute(modid, cmdid, route);
        if (route.empty())
            usleep(50000);//wait 50ms
        else
            break;
        ++retryCnt;
    }
    if (route.empty())//after 3 times retry, still can't get route, we think the module is not exist
        return -9998;// = lbagent's NOEXIST
    return 0;
}
