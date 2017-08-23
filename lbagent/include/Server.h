#ifndef __SERVER_H__
#define __SERVER_H__

#include "elb.pb.h"
#include "easy_reactor.h"

extern thread_queue<elb::GetRouteByAgentReq>* pullQueue;
extern thread_queue<elb::ReportReq>* reptQueue;

void initUDPServers();
void dssConnectorDomain(event_loop& loop);
void rptConnectorDomain();

#endif