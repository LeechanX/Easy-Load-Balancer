#include "log.h"
#include <queue>
#include "elb.pb.h"
#include "Server.h"
#include <pthread.h>
#include "easy_reactor.h"

static void newReportReq(event_loop* loop, int fd, void *args)
{
    tcp_client* cli = (tcp_client*)args;
    std::queue<elb::ReportStatusReq> msgs;
    reptQueue->recv_msg(msgs);
    while (!msgs.empty())
    {
        elb::ReportStatusReq req = msgs.front();
        msgs.pop();
        std::string reqStr;
        req.SerializeToString(&reqStr);
        cli->send_data(reqStr.c_str(), reqStr.size(), elb::GetRouteByAgentReqId);//发送消息
    }
}

static void* initRptCliIns(void* args)
{
    const char* rptIp = config_reader::ins()->GetString("reporter", "ip", "").c_str();
    short rptPort = config_reader::ins()->GetNumber("reporter", "port", 0);
    event_loop loop;
    tcp_client client(&loop, rptIp, rptPort);//创建TCP客户端
    //loop install message queue's messge coming event
    reptQueue->set_loop(&loop, newReportReq, &client);
    //run loop
    loop.process_evs();
    return NULL;
}

void rptConnectorDomain()
{
    pthread_t tid;
    int ret = ::pthread_create(&tid, NULL, initRptCliIns, NULL);
    if (ret == -1)
    {
        perror("pthread_create");
        ::exit(1);
    }
    ::pthread_detach(tid);
}
