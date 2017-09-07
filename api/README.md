## API Presentation

具体使用见api/[cpp\python]/路径下的example代码

依赖：
CPP：

        #include "elbApi.h"

Python：

        from elbApi.elbClient import elbClient

###1、创建API对象
CPP：

        elbClient client;
Python：

        client = elbClient()


###1、节点获取
#### CPP：

        int apiGetHost(int modid, int cmdid, int timo, std::string& ip, int& port)

参数：
modid + cmdid：模块标识
time：获取节点的超时时间，毫秒
ip,port：作为返回内容

返回值：
0表示获取成功；
-10000表示此模块过载；
-9998表示模块不存在；首次获取某模块时，由于lbagent尚未有此模块路由，一定会返回此错误
-9999：其他系统错误
    
#### Python：

        client.apiGetHost(modid, cmdid, timo)

参数：
modid + cmdid：模块标识
time：获取节点的超时时间，毫秒

返回值：Tuple类型：(retcode, hostOrErrmsg)

retcode同CPP返回值，
hostOrErrmsg：当retcode=0，是一个ip,port；否则是错误字符串


###2、节点调用结果汇报
#### CPP：

        void apiReportRes(int modid, int cmdid, const std::string& ip, int port, int retcode);
#### Python：
        
        client.apiReportRes(modid, cmdid, ip, port, retcode)

###3、（可选）预取路由

如果不想第一次模块获取节点返回不存在错误，可以预先调用`apiRegister`，用于事先要求lb agent拉取某模块

#### CPP：

        int elbClient::apiRegister(int modid, int cmdid);

返回值：
0表示lbagent已成功拉取到此模块；-9998表示此模块确实不存在

#### Python：

        client.apiRegister(modid, cmdid)
返回值：同上