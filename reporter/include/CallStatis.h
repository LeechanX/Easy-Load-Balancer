#ifndef __CALLSTATIS_H__
#define __CALLSTATIS_H__

#include "mysql.h"
#include "elb.pb.h"

class CallStatis
{
public:
    CallStatis();
    //no need to implement destructor
    //~CallStatis();
    void report(elb::ReportStatusReq& req);

private:
    MYSQL _dbConn;
};

#endif