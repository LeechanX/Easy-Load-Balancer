#ifndef __SERVER_H__
#define __SERVER_H__

#include "elb.pb.h"
#include "easy_reactor.h"

extern void* report2MySql(void* args);
extern int threadCnt;
extern thread_queue<elb::ReportStatusReq>** rptQueues;

#endif
