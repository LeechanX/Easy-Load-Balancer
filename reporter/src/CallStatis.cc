#include "log.h"
#include "CallStatis.h"
#include "easy_reactor.h"
#include "config_reader.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

CallStatis::CallStatis()
{
    //connection DBconn
    const char* dbHost   = config_reader::ins()->GetString("mysql", "db_host", "127.0.0.1").c_str();
    uint16_t dbPort      = config_reader::ins()->GetNumber("mysql", "db_port", 3306);
    const char* dbUser   = config_reader::ins()->GetString("mysql", "db_user", "").c_str();
    const char* dbPasswd = config_reader::ins()->GetString("mysql", "db_passwd", "").c_str();
    const char* dbName   = config_reader::ins()->GetString("mysql", "db_name", "dnsserver").c_str();

    mysql_init(&_dbConn);
    mysql_options(&_dbConn, MYSQL_OPT_CONNECT_TIMEOUT, "30");

    //when close down, let mysql_ping auto reconnect
    my_bool reconnect = 1;
    mysql_options(&_dbConn, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(&_dbConn, dbHost, dbUser, dbPasswd, dbName, dbPort, NULL, 0))
    {
        log_error("Failed to connect to MySQL[%s:%u %s %s]: %s\n", dbHost, dbPort, dbUser, dbName, mysql_error(&_dbConn));
        ::exit(1);
    }
}

void CallStatis::report(elb::ReportStatusReq& req)
{
    for (int i = 0;i < req.results_size(); ++i)
    {
        const elb::HostCallResult& result = req.results(i);
        int overload = result.overload() ? 1: 0;
        char sql[3072];
        snprintf(sql, 3072, "INSERT INTO ServerCallStatus"
            "(modid, cmdid, ip, port, caller, succ_cnt, err_cnt, ts, overload) "
            "VALUES (%d, %d, %u, %u, %u, %u, %u, %u, %d) ON DUPLICATE KEY "
            "UPDATE succ_cnt = %u, err_cnt = %u, ts = %u, overload = %d", 
            req.modid(), req.cmdid(), result.ip(), result.port(), req.caller(),
            result.succ(), result.err(), req.ts(), overload,
            result.succ(), result.err(), req.ts(), overload);

        mysql_ping(&_dbConn);//if close down, auto reconnect
        int ret = mysql_real_query(&_dbConn, sql, strlen(sql));
        if (ret)
        {
            log_error("Failed to find any records and caused an error: %s\n", mysql_error(&_dbConn));
        }
    }
}

struct Args
{
    thread_queue<elb::ReportStatusReq>* arg1;
    CallStatis* arg2;
};

static void newReportReq(event_loop* loop, int fd, void *args)
{
    thread_queue<elb::ReportStatusReq>* rptQueue = ((Args*)args)->arg1;
    CallStatis* callStat = ((Args*)args)->arg2;

    std::queue<elb::ReportStatusReq> msgs;
    rptQueue->recv_msg(msgs);
    while (!msgs.empty())
    {
        elb::ReportStatusReq req = msgs.front();
        msgs.pop();
        callStat->report(req);
    }
}

void* report2MySql(void* args)
{
    thread_queue<elb::ReportStatusReq>* rptQueue = (thread_queue<elb::ReportStatusReq>*)args;

    CallStatis callStat;
    event_loop loop;
    //loop install message queue's messge coming event
    Args cbArgs;
    cbArgs.arg1 = rptQueue;
    cbArgs.arg2 = &callStat;
    rptQueue->set_loop(&loop, newReportReq, &cbArgs);
    //run loop
    loop.process_evs();
    return NULL;
}