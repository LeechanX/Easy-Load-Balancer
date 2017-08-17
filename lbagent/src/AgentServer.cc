#include <pthread.h>
#include "log.h"
#include "elb.pb.h"
#include "Server.h"
#include "easy_reactor.h"

static void getHost(const char* data, uint32_t len, int msgid, net_commu* commu, void* usr_data)
{
    int rspMsgId = elb::GetHostRspId;
    elb::GetHostReq req;
    req.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包
    int modid = req.modid();
    int cmdid = req.cmdid();
    //.................................
    elb::GetHostRsp rsp;
    rsp.set_seq(req.seq());
    //.................................
}

static void reportStatus(const char* data, uint32_t len, int msgid, net_commu* commu, void* usr_data)
{
    elb::ReportReq req;
    req.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包
    int modid = req.modid();
    int cmdid = req.cmdid();
    int recode = req.retcode();
    int ip = req.host().ip();
    int port = req.host().port();
    //.................................
}

static void* initUDPServerIns(void* portPtr)
{
    short port = *((short*)portPtr);
    event_loop loop;
    udp_server server(&loop, "127.0.0.1", port);//创建UDP服务器
    server.add_msg_cb(elb::GetHostReqId, getHost);//设置：当收到消息id = GetHostReqId的消息调用的回调函数getHost
    server.add_msg_cb(elb::ReportReqId, reportStatus);//设置：当收到消息id = ReportReqId的消息调用的回调函数reportStatus
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