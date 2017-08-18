#include "log.h"
#include "logo.h"
#include "elb.pb.h"
#include "Server.h"
#include <pthread.h>
#include "easy_reactor.h"

thread_queue<elb::GetRouteByAgentReq>* pullQueue = NULL;
thread_queue<elb::ReportReq>* reptQueue = NULL;

int main()
{
    config_reader::setPath("lbagent.ini");
    dispLogo();
    _init_log_("lbagent", ".");
    int log_level = config_reader::ins()->GetNumber("log", "level", 3);
    _set_log_level_(log_level);

    pullQueue = new thread_queue<elb::GetRouteByAgentReq>();
    if (!pullQueue)
    {
        log_error("no space to create thread_queue<elb::GetRouteByAgentReq>");
        return 1;
    }

    reptQueue = new thread_queue<elb::ReportReq>();
    if (!reptQueue)
    {
        log_error("no space to create thread_queue<elb::ReportReq>");
        return 1;
    }

    //init three UDP servers and create three thread for localhost[8888~8890] run in loop
    initUDPServers();
    //init connector who connects to reporter, create a thread and run in loop
    rptConnectorDomain();
    //init connector who connects to dns server, and run in loop
    dssConnectorDomain();
    return 0;
}
