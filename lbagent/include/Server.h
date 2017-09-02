#ifndef __SERVER_H__
#define __SERVER_H__

#include "elb.pb.h"
#include "RouteLb.h"
#include "easy_reactor.h"

enum RETCODE
{
    OVERLOAD = -10000,
    SYSERR,//-9999
    NOEXIST,//-9998
    SUCCESS = 0,
};

extern thread_queue<elb::GetRouteReq>* pullQueue;
extern thread_queue<elb::ReportStatusReq>* reptQueue;

extern RouteLB routeLB[3];

void initUDPServers();
void dssConnectorDomain(event_loop& loop);
void rptConnectorDomain();

#endif