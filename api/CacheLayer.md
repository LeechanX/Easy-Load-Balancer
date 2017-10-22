## API Cache Layer

使用ELB发现，某台业务较多的线上服务器有时会抖动，于是有少量时刻会造成业务获取节点超时，为解决此问题，提出在业务端增加缓存层

除了解决问题外，提供缓存层还基于一个考量：
>在模块节点都正常时（无过载），业务没有必要每次远程调用时候都特地向lbagent获取节点，而可先把模块路由缓存，自主轮询，如此还可以节省大量的QPS lbagent消耗

于是，缓存层设计的大思路就是：
对于某mod，如果其下路由无节点过载，优先走缓存；如有节点过载，则仍走原来的方式：本地网络

### 协议支持

    //为支持API cache: api向agent发起获取\更新路由请求
    message CacheGetRouteReq {
        required int32 modid   = 1;
        required int32 cmdid   = 2;
        required int64 version = 3;//首次获取时，version=-1
    }
    //为支持API cache: agent给api返回的路由应答
    message CacheGetRouteRsp {
        required int32 modid    = 1;
        required int32 cmdid    = 2;
        required int64 version  = 3;//agent不存在此mod，则version=-1
        optional bool overload  = 4;
        repeated HostAddr route = 5;
    }
    //为支持API cache: api向agent批量上报若干成功结果
    message CacheBatchRptReq {
        required int32 modid              = 1;
        required int32 cmdid              = 2;
        repeated HostBatchCallRes results = 3;
    }

### 带缓存的节点获取流程

要对某mod获取节点

1、如果此mod不存在于缓存中，则打包`CacheGetRouteReq`请求向lbagent获取路由：
lbagent返回`CacheGetRouteRsp`类型的rsp，如果：
- rsp.version = -1，说明lbagent端无此mod路由，于是返给业务：不存在错误
- 否则添加到本地缓存，包括版本号、路由信息、是否有节点过载等rsp携带的信息，然后走step 3

2、如果此mod存在于缓存中，但此mod上次更新时间距今超过2s，则打包`CacheGetRouteReq`请求向lbagent更新路由：
lbagent返回`CacheGetRouteRsp`类型的rsp，如果：
- rsp.version = -1，说明lbagent端此mod路由已删除，于是删除对应缓存，且返给业务：不存在错误
- 否则添加到本地缓存，包括版本号、是否有节点过载等rsp携带的信息，如果`rsp.version!=`本地缓存版本则还要更新路由信息，然后走step 3

3、此mod存在于缓存中，但是有节点过载：则走原逻辑：本地网络向lbagent获取节点

4、此mod存在于缓存中，且无节点过载，则本地缓存轮询一个节点返给业务

### 带缓存的状态上报流程

本地缓存永远只暂存调用成功的结果

对于待上报条目`modid, cmdid, ip, port, retcode`
- 如果此mod无节点处于过载：
 - 如果`retcode = 0`，且此mod无节点处于过载，则暂存到缓存中即可
 - 如果`retcode != 0`，将之前缓存的此mod下所有节点调用结果打包成请求`CacheBatchRptReq`批量上报给lbagent；然后将本次调用结果走原来的本地网络方式上报给lbagent

- 如果此mod有节点处于过载：
 - 将本次调用结果走原来的本地网络方式上报给lbagent

此外：
- 在mod的节点获取流程中，如果本地缓存时效超时，则先把此mod暂存的调用结果先批量上报给lbagent，然后才发起更新路由请求`CacheGetRouteReq`
- 在业务退出时，ELB客户端析构函数中会把缓存中每个mod暂存的调用结果批量上报给lbagent

**“暂存0、立即发非0”** 的方式，可以保证两种调用结果（0、非0）的产生顺序不被缓存逻辑打乱，不影响lbagent的LB算法的过载发现、恢复逻辑；
而如果将无论0、非0调用结果都暂存到缓存，则无法高效地记录下这些调用结果的先后顺序（如100个0,50个非0，要准确记录这150个调用结果的产生顺序需要150长度的数组）

>**调用结果顺序如何影响lbagent的LB算法过载发现、恢复逻辑？**
>
>假设节点H产生的调用结果是100个0+50个1,
>
>如果产生顺序是先50个非0，而后100个1，则lbagent会在连续15个1后判断H过载；又在连续15个0后判断H恢复
>
>如果产生的顺序是0101010101010......，则lbagent可能一直不认为H过载；
>
>于是说：调用结果顺序将影响lbagent的LB算法

### 评估

优点：
- 1、节约agent QPS，提高了业务调用ELB的响应能力；
- 2、不影响LB算法的过载发现、恢复逻辑

缺点：
- 暂未发现
