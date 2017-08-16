#include <string>
#include <stdlib.h>
#include <pthread.h>
#include "log.h"
#include "logo.h"
#include "Route.h"
#include "elb.pb.h"
#include "easy_reactor.h"

void getRoute(const char* data, uint32_t len, int msgid, net_commu* commu, void* usr_data)
{
    int rspMsgId = elb::GetRouteByAgentRspId;
    elb::GetRouteByAgentReq req;
    elb::GetRouteByAgentRsp rsp;
    req.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包

    if (!req.ParseFromArray(data, len))
    {
        log_error("request decode error");
        return ;
    }

    int modid = req.modid(), cmdid = req.cmdid();
    hostSet hosts = Singleton<Route>::ins()->getHosts(modid, cmdid);

    rsp.set_modid(modid);
    rsp.set_cmdid(cmdid);

    for (hostSetIt it = hosts.begin();it != hosts.end(); ++it)
    {
        uint64_t key = *it;
        elb::HostAddr host;
        host.set_ip((uint32_t)(key >> 32));
        host.set_port((int)key);
        rsp.add_hosts()->CopyFrom(host);
    }

    std::string rspStr;
    rsp.SerializeToString(&rspStr);
    commu->send_data(rspStr.c_str(), rspStr.size(), rspMsgId);//回复消息
}

int main()
{
    event_loop loop;

    config_reader::setPath("dnsserver.ini");
    std::string ip = config_reader::ins()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_reader::ins()->GetNumber("reactor", "port", 12315);

    dispLogo();

    tcp_server server(&loop, ip.c_str(), port);//创建TCP服务器
    server.add_msg_cb(elb::GetRouteByAgentReqId, getRoute);//设置：当收到消息id = 1的消息调用的回调函数  我们约定EchoString消息的ID是1

    _init_log_("dnsserver", ".");
    int log_level = config_reader::ins()->GetNumber("log", "level", 3);
    _set_log_level_(log_level);

    //thread: 周期性加载路由数据
    pthread_t tid;
    int ret = ::pthread_create(&tid, NULL, dataLoader, NULL);
    if (ret == -1)
    {
        perror("pthread_create");
        ::exit(1);
    }
    ::pthread_detach(tid);

    loop.process_evs();
    return 0;
}
