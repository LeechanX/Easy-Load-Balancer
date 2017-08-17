#include <pthread.h>
#include "log.h"
#include "logo.h"
#include "elb.pb.h"
#include "Server.h"
#include "easy_reactor.h"

int main()
{
    config_reader::setPath("lbagent.ini");
    dispLogo();
    _init_log_("lbagent", ".");
    int log_level = config_reader::ins()->GetNumber("log", "level", 3);
    _set_log_level_(log_level);

    //init three UDP servers and three thread for localhost[8888~8890]
    initUDPServers();
    while (1)
    {
        sleep(3);
    }
    return 0;
}
