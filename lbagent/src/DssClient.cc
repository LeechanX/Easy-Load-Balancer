#include "log.h"
#include <queue>
#include "elb.pb.h"
#include "Server.h"
#include "easy_reactor.h"

static void recvRoute(const char* data, uint32_t len, int msgid, net_commu* commu, void* usr_data)
{
    elb::GetRouteRsp rsp;
    rsp.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包
    int modid = rsp.modid();
    int cmdid = rsp.cmdid();
    int index = (modid + cmdid) % 3;
    //update metadata
    routeLB[index]->update(modid, cmdid, rsp);
}

static void newPullReq(event_loop* loop, int fd, void *args)
{
    tcp_client* cli = (tcp_client*)args;
    std::queue<elb::GetRouteReq> msgs;
    pullQueue->recv_msg(msgs);
    while (!msgs.empty())
    {
        elb::GetRouteReq req = msgs.front();
        msgs.pop();
        std::string reqStr;
        req.SerializeToString(&reqStr);
        cli->send_data(reqStr.c_str(), reqStr.size(), elb::GetRouteByAgentReqId);//发送消息
    }
}

static void whenConnected(tcp_client* client, void* args)
{
    for (int i = 0;i < 3; ++i)
        routeLB[i]->clearPulling();
}

void dssConnectorDomain(event_loop& loop)
{
    const char* dssIp = config_reader::ins()->GetString("dnsserver", "ip", "").c_str();
    short dssPort = config_reader::ins()->GetNumber("dnsserver", "port", 0);
    tcp_client client(&loop, dssIp, dssPort, "dnsserver");//创建TCP客户端

    //设置：当收到消息id=1的消息时的回调函数
    client.add_msg_cb(elb::GetRouteByAgentRspId, recvRoute/*, ???*/);

    //设置：连接成功、断线重连接成功后调用whenConnClose来清理之前的任务
    client.onConnection(whenConnected);

    //loop install message queue's messge coming event
    pullQueue->set_loop(&loop, newPullReq, &client);
    //run forever
    loop.process_evs();
}
