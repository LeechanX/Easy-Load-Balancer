#include <pthread.h>
#include "log.h"
#include "logo.h"
#include "elb.pb.h"
#include "easy_reactor.h"

void getHost(const char* data, uint32_t len, int msgid, net_commu* commu, void* usr_data)
{
    int rspMsgId = elb::GetHostRspId;
    elb::GetHostReq req;//GetHostReqId
    elb::GetHostRsp rsp;
    req.ParseFromArray(data, len);//解包，data[0:len)保证是一个完整包
}

int main()
{
    config_reader::setPath("lbagent.ini");
    dispLogo();
    _init_log_("lbagent", ".");
    int log_level = config_reader::ins()->GetNumber("log", "level", 3);
    _set_log_level_(log_level);

    
    
    return 0;
}
