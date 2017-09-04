#include <pthread.h>
#include "log.h"
#include "elb.pb.h"
#include "Server.h"
#include "easy_reactor.h"

static void getHost(const char* data, uint32_t len, int msgid, net_commu* commu, void* usrData)
{
    elb::GetHostReq req;
    req.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包
    int modid = req.modid();
    int cmdid = req.cmdid();
    //response
    elb::GetHostRsp rsp;
    rsp.set_seq(req.seq());
    rsp.set_modid(modid);
    rsp.set_cmdid(cmdid);
    //get host from route lb metadata
    RouteLB* ptrRouteLB = (RouteLB*)usrData;
    ptrRouteLB->getHost(modid, cmdid, rsp);
    std::string rspStr;
    rsp.SerializeToString(&rspStr);
    commu->send_data(rspStr.c_str(), rspStr.size(), elb::GetHostRspId);//回复消息
}

static void reportStatus(const char* data, uint32_t len, int msgid, net_commu* commu, void* usrData)
{
    elb::ReportReq req;
    req.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包
    //report to route lb metadata
    RouteLB* ptrRouteLB = (RouteLB*)usrData;
    ptrRouteLB->report(req);
}

static void getRoute(const char* data, uint32_t len, int msgid, net_commu* commu, void* usrData)
{
    elb::GetRouteReq req;
    req.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包
    //report to route lb metadata
    RouteLB* ptrRouteLB = (RouteLB*)usrData;
    elb::GetRouteRsp rsp;
    rsp.set_modid(req.modid());
    rsp.set_cmdid(req.cmdid());
    ptrRouteLB->getRoute(req.modid(), req.cmdid(), rsp);
    std::string rspStr;
    rsp.SerializeToString(&rspStr);
    commu->send_data(rspStr.c_str(), rspStr.size(), elb::GetRouteByToolRspId);//回复消息
}

static void persistRoute(event_loop* loop, void* usrData)
{
    RouteLB* ptrRouteLB = (RouteLB*)usrData;
    ptrRouteLB->persistRoute();
}

static void* initUDPServerIns(void* portPtr)
{
    short port = *((short*)portPtr);
    event_loop loop;
    udp_server server(&loop, "127.0.0.1", port);//创建UDP服务器

    server.add_msg_cb(elb::GetHostReqId, getHost, routeLB[port - 8888]);//设置：当收到消息id = GetHostReqId的消息调用的回调函数getHost
    server.add_msg_cb(elb::ReportReqId, reportStatus, routeLB[port - 8888]);//设置：当收到消息id = ReportReqId的消息调用的回调函数reportStatus
    server.add_msg_cb(elb::GetRouteByToolReqId, getRoute, routeLB[port - 8888]);//设置：当收到消息id = GetRouteByToolReqId的消息调用的回调函数getRoute

    loop.run_every(persistRoute, routeLB[port - 8888], 60);//设置：每隔60s将本地已拉到的路由持久化到磁盘

    loop.process_evs();
    return NULL;
}

void initUDPServers()
{
    static short ports[3] = {8888, 8889, 8890};
    for (int i = 0;i < 3; ++i)
    {
        pthread_t tid;
        int ret = ::pthread_create(&tid, NULL, initUDPServerIns, &ports[i]);
        if (ret == -1)
        {
            perror("pthread_create");
            ::exit(1);
        }
        ::pthread_detach(tid);
    }
}