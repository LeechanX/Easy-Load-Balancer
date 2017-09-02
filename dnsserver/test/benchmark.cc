#include <stdio.h>
#include <time.h>
#include <string>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "elb.pb.h"
#include "easy_reactor.h"

struct Config
{
    Config(): hostip(NULL), hostPort(0), concurrency(0), total(0) {}
    char* hostip;
    short hostPort;
    int concurrency;
    long total;
};

unsigned long getCurrentMills()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

Config config;
unsigned long startTs, endTs;

void parseOption(int argc, char** argv)
{
    for (int i = 0;i < argc; ++i)
    {
        if (!strcmp(argv[i], "-h"))
        {
            config.hostip = argv[i + 1];
        }
        else if (!strcmp(argv[i], "-p"))
        {
            config.hostPort = atoi(argv[i + 1]);
        }
        else if (!strcmp(argv[i], "-c"))
        {
            config.concurrency = atoi(argv[i + 1]);
        }
        else if (!strcmp(argv[i], "-n"))
        {
            config.total = atol(argv[i + 1]);
        }
    }
    if (!config.hostip || !config.hostPort || !config.concurrency || !config.total)
    {
        printf("./dss-benchmark -h ip -p port -c concurrency -n total\n");
        exit(1);
    }
}

void ActionGetRoute(const char* data, uint32_t len, int msgId, net_commu* commu, void* usr_data)
{
    long* count = (long*)usr_data;
    elb::GetRouteRsp rsp;
    //解包，data[0:len)保证是一个完整包
    rsp.ParseFromArray(data, len);

    *count = *count + 1;
    if (*count >= config.total)
    {
        endTs = getCurrentMills();
        printf("communicate %ld times\n", *count);
        printf("time use %ldms\n", endTs - startTs);
        printf("qps %.2f\n", (*count * 1000.0) / (endTs - startTs));
        exit(1);
    }

    int modid = rsp.modid();
    int cmdid = rsp.cmdid();
    elb::GetRouteReq req;
    req.set_modid(modid);
    req.set_cmdid(cmdid);
    std::string reqStr;
    req.SerializeToString(&reqStr);
    commu->send_data(reqStr.c_str(), reqStr.size(), elb::GetRouteByAgentReqId);//回复消息
}

void onConnectionCb(tcp_client* client, void *args)
{
    unsigned long* startTsPtr = (unsigned long*)args;
    if (!*startTsPtr)
        *startTsPtr = getCurrentMills();
    //连接建立后，主动发送消息
    elb::GetRouteReq req;
    req.set_modid(10001);
    req.set_cmdid(1001);
    std::string reqStr;
    req.SerializeToString(&reqStr);
    client->send_data(reqStr.c_str(), reqStr.size(), elb::GetRouteByAgentReqId);
}

int main(int argc, char** argv)
{
    parseOption(argc, argv);
    long done = 0;
    event_loop loop;
    std::vector<tcp_client*> clients;
    for (int i = 0;i < config.concurrency; ++i)
    {
        tcp_client* cli = new tcp_client(&loop, config.hostip, config.hostPort);//创建TCP客户端
        if (!cli) exit(1);
        cli->add_msg_cb(elb::GetRouteByAgentRspId, ActionGetRoute, &done);//设置：当收到消息id=GetRouteByAgentRspId的消息时的回调函数
        cli->onConnection(onConnectionCb, &startTs);//当连接建立后，执行函数onConnectionCb
        clients.push_back(cli);
    }

    loop.process_evs();

    for (int i = 0;i < config.concurrency; ++i)
    {
        tcp_client* cli = clients[i];
        delete cli;
    }
    return 0;
}
