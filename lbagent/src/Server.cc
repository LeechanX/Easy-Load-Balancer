#include "log.h"
#include "logo.h"
#include "elb.pb.h"
#include "Server.h"
#include "HeartBeat.h"
#include <pthread.h>
#include "easy_reactor.h"

thread_queue<elb::GetRouteByAgentReq>* pullQueue = NULL;
thread_queue<elb::ReportReq>* reptQueue = NULL;

RouteLB routeLB[3];

//timeout event 1: record current time in shared memory
static void recordTs(event_loop* loop, void* usrData)
{
    HeartBeat* hb = (HeartBeat*)usrData;
    hb->recordTs();
}

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

    event_loop mainLoop;
    //init connector who connects to dns server, and run in loop [main thread]
    dssConnectorDomain(mainLoop);
    //install timeout event 1: record current time in shared memory, 1 second 1 do
    HeartBeat hb(true);
    mainLoop.run_every(recordTs, &hb, 1);
    //run loop
    mainLoop.process_evs();
    return 0;
}
