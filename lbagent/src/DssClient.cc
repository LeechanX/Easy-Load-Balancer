#include "log.h"
#include <queue>
#include "elb.pb.h"
#include "Server.h"
#include "easy_reactor.h"

static void recvRoute(const char* data, uint32_t len, int msgid, net_commu* commu, void* usr_data)
{
    elb::GetRouteByAgentRsp rsp;
    rsp.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包
    int modid = rsp.modid();
    int cmdid = rsp.cmdid();
    //.................................
}

static void newPullReq(event_loop* loop, int fd, void *args)
{
    tcp_client* cli = (tcp_client*)args;
    std::queue<elb::GetRouteByAgentReq> msgs;
    pullQueue->recv_msg(msgs);
    while (!msgs.empty())
    {
        elb::GetRouteByAgentReq req = msgs.front();
        std::string reqStr;
        req.SerializeToString(&reqStr);
        cli->send_data(reqStr.c_str(), reqStr.size(), elb::GetRouteByAgentReqId);//发送消息
    }
}

void dssConnectorDomain()
{
    const char* dssIp = config_reader::ins()->GetString("dnsserver", "ip", "").c_str();
    short dssPort = config_reader::ins()->GetNumber("dnsserver", "port", 0);
    event_loop loop;
    tcp_client client(&loop, dssIp, dssPort);//创建TCP客户端
    client.add_msg_cb(elb::GetRouteByAgentRspId, recvRoute/*, ???*/);//设置：当收到消息id=1的消息时的回调函数
    //loop install message queue's messge coming event
    pullQueue->set_loop(&loop, newPullReq, &client);
    //run loop
    loop.process_evs();
}
