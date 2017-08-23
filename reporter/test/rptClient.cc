#include <string>
#include <stdlib.h>
#include <pthread.h>
#include "elb.pb.h"
#include "easy_reactor.h"

void reportStatus(event_loop* loop, void* usr_data)
{
    tcp_client* client = (tcp_client*)usr_data;

    elb::ReportStatusReq req;
    req.set_modid(rand() % 3 + 1);
    req.set_cmdid(1);
    req.set_caller(123);
    req.set_ts(time(NULL));
    req.set_overload(true);
    for (int i = 0;i < 3; ++i)
    {
        elb::HostCallResult res;
        res.set_ip(i + 1);
        res.set_port((i + 1) * (i + 1));
        res.set_succ(100);
        res.set_err(3);
        req.add_results()->CopyFrom(res);
    }

    std::string reqStr;
    req.SerializeToString(&reqStr);
    client->send_data(reqStr.c_str(), reqStr.size(), elb::ReportStatusReqId);//主动发送消息
}

void whenConnectDone(tcp_client* client, void* args)
{
    event_loop* loop = client->loop();
    loop->run_every(reportStatus, client, 1);    
}

int main(int argc, char const *argv[])
{
    srand(time(NULL));
    event_loop loop;
    tcp_client client(&loop, "127.0.0.1", 12316);//创建TCP客户端
    client.setup_connectcb(whenConnectDone);
    loop.process_evs();
    return 0;
}
