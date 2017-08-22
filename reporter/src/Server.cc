#include <string>
#include <stdlib.h>
#include <pthread.h>
#include "log.h"
#include "logo.h"
#include "util.h"
#include "Server.h"
#include "elb.pb.h"
#include "easy_reactor.h"

int threadCnt = 0;
thread_queue<elb::ReportStatusReq>** rptQueues = NULL;

void reportStatus(const char* data, uint32_t len, int msgid, net_commu* commu, void* usr_data)
{
    elb::ReportStatusReq req;
    req.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包

    if (!req.ParseFromArray(data, len))
    {
        log_error("request decode error");
        return ;
    }
    int modid = req.modid();
    int cmdid = req.cmdid();
    uint64_t key = ((uint64_t)modid<<32) + cmdid;
    int index = HASHTO(&key, threadCnt);
    rptQueues[index]->send_msg(req);
}

int main()
{
    event_loop loop;

    config_reader::setPath("reporter.ini");
    std::string ip = config_reader::ins()->GetString("reactor", "ip", "0.0.0.0");
    short port = config_reader::ins()->GetNumber("reactor", "port", 12316);

    dispLogo();

    tcp_server server(&loop, ip.c_str(), port);//创建TCP服务器
    server.add_msg_cb(elb::ReportStatusReqId, reportStatus);//设置：当收到消息id = ReportStatusReqId （即上报调用结果）的消息调用的回调函数

    _init_log_("reporter", ".");
    int log_level = config_reader::ins()->GetNumber("log", "level", 3);
    _set_log_level_(log_level);

    threadCnt = config_reader::ins()->GetNumber("mysql", "thread_cnt", 3);
    rptQueues = new thread_queue<elb::ReportStatusReq>*[threadCnt];
    if (!rptQueues)
    {
        log_error("no space to create thread_queue<elb::ReportStatusReq>*[%d]", threadCnt);
        return 1;
    }
    for (int i = 0;i < threadCnt; ++i)
    {
        rptQueues[i] = new thread_queue<elb::ReportStatusReq>();
        if (!rptQueues[i])
        {
            log_error("no space to create thread_queue<elb::ReportStatusReq>");
            return 1;
        }
        pthread_t tid;
        int ret = ::pthread_create(&tid, NULL, report2MySql, rptQueues[i]);
        if (ret == -1)
        {
            perror("pthread_create");
            ::exit(1);
        }
        ::pthread_detach(tid);
    }

    loop.process_evs();
    return 0;
}
