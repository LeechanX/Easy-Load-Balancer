## DNS Server Presentation
### **feature**
负责接收各agent对某modid、cmdid的请求并返回该modid、cmdid下的所有节点，即为agent提供获取路由服务
### **principle arch**
![Alt text](pictures/DnsServer-Arch.png)
### **server model**
DnsServer服务模型采用了one loop per thread TCP服务器：
- 主线程Accepter负责接收连接（agent端连接）
- Thread loop们负责处理连接的请求、回复；（agent端发送查询请求，期望获取结果）

### **DB information**

DnsServerRoute: 保存了所有mod路由信息

RouteVersion: 当前DnsServerRoute路由版本号，每次管理端修改某mod的路由，RouteVersion表中的版本号都被更新为当前时间戳

ChangeLog: 每次管理端修改某mod的路由，会记录本次对哪个mod进行修改（增、删、改），以便指示最新的DnsServerRoute路由有哪些mod变更了

### **business model**
DnsServer使用两个map存储路由数据（key = `modid<<32 + cmdid` ， value = set of `ip<<32 + port`）
- 一个map：主数据，查询请求在此map执行
- 另一个map：后台线程周期性重加载路由到此map，作为最新数据替换掉上一个map

这两个map分别由指针`data_ptr`与`tmp_ptr`指向

#### **dnsserver还有个业务线程:**

- Backend Thread：
负责周期性（default:1s）检查RouteVersion表版本号，如有变化，说明DnsServerRoute有变更，则重加载DnsServerRoute表内容；然后将ChangeLog表中被变更的mod取出，根据订阅列表查出mod被哪些连接订阅后，向所有工作线程发送任务：要求订阅这些mod的连接推送mod路由到agent

此外，还负责周期性（default:8s）重加载DnsServerRoute表内容

**PS:重加载DnsServerRoute表内容的细节**

重加载DnsServerRoute表内容到`tmp_ptr`指向的map，而后上写锁，交换指针`data_ptr`与`tmp_ptr`的地址，于是完成了路由数据更新

### **in service**
服务启动时，DnsServerRoute表被加载到`data_ptr`指向的map中, `tmp_ptr`指向的map为空

服务启动后，agent发来Query for 某modid/cmdid，其所在Thread Loop上，上读锁查询`data_ptr`指向的map，返回查询结果；
顺便如果此moid,cmdid不存在，则把agent ip+agent port+此moid+cmdid发送到Backend thread loop1的队列，让其记录到ClientMap

后台线程Backend thread每隔10s清空`tmp_ptr`指向的map，再加载DnsServerRoute表内容到`tmp_ptr`指向的map，加载成功后交换指针`data_ptr`与`tmp_ptr`指针内容，于是完成了路由数据的更新

### **performance**

>服务器参数：
>CPU个数：24   内存：128GB   网卡队列个数：24

**QPS测试结果：**

| dnsserverTCP服务线程数 |  benchmark情况 |  QPS |
| :-----: | :-----: | :-----: |
|3线程|6个benchmark，各建立100个连接| `25.83W/s` |
|5线程|6个benchmark，各建立100个连接| `39.4W/s` |
