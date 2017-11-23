#include <string>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "log.h"
#include "logo.h"
#include "Route.h"
#include "elb.pb.h"
#include "Server.h"
#include "SubscribeList.h"

tcp_server* server;

void getRoute(const char* data, uint32_t len, int msgid, net_commu* com, void* usr_data)
{
    elb::GetRouteReq req;
    elb::GetRouteRsp rsp;
    req.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包

    if (!req.ParseFromArray(data, len))
    {
        log_error("request decode error");
        return ;
    }

    int modid = req.modid(), cmdid = req.cmdid();

    //如果之前没有订阅过此mod，就订阅
    uint64_t key = (((uint64_t)modid) << 32) + cmdid;
    Interest* book = (Interest*)com->parameter;
    if (book->m.find(key) == book->m.end())
    {
        book->m.insert(key);
        //记录到全局订阅列表
        Singleton<SubscribeList>::ins()->subscribe(key, com->get_fd());
    }

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
    com->send_data(rspStr.c_str(), rspStr.size(), elb::GetRouteByAgentRspId);//回复消息
}

void createSubscribe(net_commu* com)
{
    com->parameter = new Interest();
    assert(com->parameter);
}

void clearSubscribe(net_commu* com)
{
    std::set<uint64_t>::iterator it;
    Interest* book = (Interest*)com->parameter;
    for (it = book->m.begin();it != book->m.end(); ++it)
    {
        uint64_t mod = *it;
        Singleton<SubscribeList>::ins()->unsubscribe(mod, com->get_fd());
    }
    delete book;
    com->parameter = NULL;
}

void pushChange(event_loop* loop, void* args)
{
    __gnu_cxx::hash_set<int> listening;
    __gnu_cxx::hash_map<int, __gnu_cxx::hash_set<uint64_t> > news;
    __gnu_cxx::hash_map<int, __gnu_cxx::hash_set<uint64_t> >::iterator it;
    __gnu_cxx::hash_set<uint64_t>::iterator st;

    loop->nlistenings(listening);//获取当前loop所有连接
    //从订阅列表取走所有待push消息
    Singleton<SubscribeList>::ins()->fetchPush(listening, news);
    for (it = news.begin();it != news.end(); ++it)
    {
        int fd = it->first;
        for (st = it->second.begin();st != it->second.end(); ++st)
        {
            int modid = (int)((*st) >> 32);
            int cmdid = (int)(*st);

            elb::GetRouteRsp rsp;
            rsp.set_modid(modid);
            rsp.set_cmdid(cmdid);

            hostSet hosts = Singleton<Route>::ins()->getHosts(modid, cmdid);
            for (hostSetIt ht = hosts.begin();ht != hosts.end(); ++ht)
            {
                uint64_t key = *ht;
                elb::HostAddr host;
                host.set_ip((uint32_t)(key >> 32));
                host.set_port((int)key);
                rsp.add_hosts()->CopyFrom(host);
            }
            std::string rspStr;
            rsp.SerializeToString(&rspStr);
            net_commu* com = tcp_server::conns[fd];
            com->send_data(rspStr.c_str(), rspStr.size(), elb::GetRouteByAgentRspId);//回复消息
        }
    }
}

int main()
{
    event_loop loop;

    config_reader::setPath("dnsserver.ini");
    std::string ip = config_reader::ins()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_reader::ins()->GetNumber("reactor", "port", 12315);

    dispLogo();
    server = new tcp_server(&loop, ip.c_str(), port);//创建TCP服务器
    if (!server)
    {
        perror("malloc new tcp_server");
        ::exit(1);
    }

    //设置：当收到消息id = GetRouteByAgentReqId （即获取路由）的消息调用的回调函数
    server->add_msg_cb(elb::GetRouteByAgentReqId, getRoute);

    //当连接建立，调用函数createSubscribe,创建保存自己所订阅mod的集合
    server->onConnBuild(createSubscribe);
    //当连接关闭，调用函数,删除自己所订阅mod
    server->onConnClose(clearSubscribe);

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
