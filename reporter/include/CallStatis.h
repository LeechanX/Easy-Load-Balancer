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

    //多线程使用Mysql需要先调用mysql_library_init
    static void libraryInit() { mysql_library_init(0, NULL, NULL); }
private:
    MYSQL _dbConn;
};

#endif