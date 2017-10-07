#encoding=utf-8

import os
import sys
import mmap
import time
import socket
import struct
import elb_pb2
from StaticRoute import StaticRoute
from CacheLayer import CacheUnit

class FormatError(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

class elbClient:
    def __init__(self):
        self._socks = [None for i in range(3)]
        try:
            self._socks[0] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self._socks[1] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self._socks[2] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        except socket.error, errMsg:
            print errMsg
            exit(1)
        self.__seq = 0
        self.__f = os.open('/tmp/hb_map.bin', os.O_RDONLY)
        self.__m = mmap.mmap(self.__f, 8, flags = mmap.MAP_SHARED, prot = mmap.PROT_READ, offset = 0)
        self.__staticRoute = StaticRoute()
        self.__agentOff = True
        self.cache = {}

    def __del__(self):
        for key in self.cache:
            modid, cmdid = key
            self.__batchReportRes(self.cache[(modid, cmdid)])
        for sock in self._socks:
            sock.close()
        os.close(self.__f)

    def agentDie(self):
        currTs = int(time.time())
        s = self.__m.read(8)
        ts = struct.unpack('q', s)[0]
        self.__m.seek(0)
        return currTs - ts > 2

    def apiGetHost(self, modid, cmdid, timo):
        if self.agentDie():
            self.__agentOff = True
            staticData = self.__staticRoute.getHost(modid, cmdid)
            if staticData == -1:
                return (-9998, 'no exist')
            return (0, staticData)
        self.__staticRoute.freeData()
        self.__agentOff = False
        #get host from cache
        currTs = int(time.time())
        if (modid, cmdid) not in self.cache:
            #此mod不在缓存中，则向agent获取
            ret, err = self.__getRoute4Cache(modid, cmdid, currTs)
            if ret:
                return (ret, err)
        #从缓存中获取刚拉到的此mod条目
        cacheItem = self.cache.get((modid, cmdid), None)
        if cacheItem is None:
            return (-9998, 'no exist')

        #检查此mod的缓存条目是否超时：
        #如果上次更新是2s前，则重拉取一次
        #顺便先把暂存的上报信息报上去（如果有的话）
        if currTs - cacheItem.lstUpdTs >= 2:
            self.__batchReportRes(cacheItem)
            self.__getRoute4Cache(modid, cmdid, currTs)
            #从缓存中获取刚拉到的此mod条目
            cacheItem = self.cache.get((modid, cmdid), None)
        if cacheItem is None:
            #说明agent端已经不存在此mod了，返回不存在错误
            return (-9998, 'no exist')

        #如果此mod在API端没有overload节点，则从缓存中获取节点，否则走网络
        if not cacheItem.overload:
            ip, port = cacheItem.getHost()
            return (0, (ip, port))
        #get host from local network
        if timo < 10: timo = 10
        if timo > 1000: timo = 1000
        i = (modid + cmdid) % 3
        sock = self._socks[i]
        if self.__seq == 2 ** 31: self.__seq = 0
        #create request
        req = elb_pb2.GetHostReq()
        req.seq = self.__seq
        self.__seq += 1
        req.modid = modid
        req.cmdid = cmdid
        rsp = elb_pb2.GetHostRsp()
        #send request
        bodyStr = req.SerializeToString()
        reqStr = struct.pack('i', elb_pb2.GetHostReqId) + struct.pack('i', len(bodyStr)) + bodyStr
        try:
            sock.sendto(reqStr, ('127.0.0.1', 8888 + i))
        except socket.error, errMsg:
            print >> sys.stderr, errMsg
            return (-9999, errMsg)
        try:
            #recv response
            sock.settimeout(timo * 0.001)
            rspStr, addr = sock.recvfrom(65536)
            rspId = struct.unpack('i', rspStr[:4])[0]
            bodyLen = struct.unpack('i', rspStr[4:8])[0]
            if rspId != elb_pb2.GetHostRspId or bodyLen != len(rspStr[8:]):
                raise FormatError('message head format error')
            rsp.ParseFromString(rspStr[8:])
            while rsp.seq < req.seq:
                rspStr, addr = sock.recvfrom(65536)
                rspId = struct.unpack('i', rspStr[:4])[0]
                bodyLen = struct.unpack('i', rspStr[4:8])[0]
                if rspId != elb_pb2.GetHostReqId or bodyLen != len(rspStr[8:]):
                    raise FormatError('message head format error')
                rsp.ParseFromString(rspStr[8:])
            if rsp.seq != req.seq:
                print >> sys.stderr, 'request seq id is %d, response seq id is %d' % (req.seq, rsp.seq)
                return (-9999, 'request seq id is %d, response seq id is %d' % (req.seq, rsp.seq))
            elif rsp.retcode != 0:
                errMsg = ''
                if rsp.retcode == -10000:
                    errMsg = 'overload'
                elif rsp.retcode == -9998:
                    errMsg = 'no exist,slslslslslslsls'
                else:
                    errMsg = 'agent error'
                return (rsp.retcode, errMsg)
            else:
                ipn = rsp.host.ip
                if ipn < 0:
                    ipn += 2 ** 32
                ips = socket.inet_ntoa(struct.pack('I', ipn))
                return (0, (ips, rsp.host.port))
        except socket.timeout:
            print >> sys.stderr, 'time out when recvfrom socket'
            return (-9999, 'time out when recvfrom socket')
        except FormatError as e:
            return (-9999, e.value)

    def apiReportRes(self, modid, cmdid, ip, port, res):
        if self.__agentOff:
            return
        cu = self.cache.get((modid, cmdid), None)
        if cu and not cu.overload:
            if res == 0:
                #report to cache
                cu.report(ip, port)
                return
            else:
                #retcode != 0 立即将之前暂存的report状态上报予agent，如果有的话
                self.__batchReportRes(cu)

        #report by local network
        i = (modid + cmdid) % 3
        sock = self._socks[i]
        #create request
        req = elb_pb2.ReportReq()
        req.modid = modid
        req.cmdid = cmdid
        req.retcode = res
        #ip is str, but req.host.ip need big-endian number
        ipn = struct.unpack('I', socket.inet_aton(ip))[0]
        if ipn > 2 ** 31 - 1:
            ipn -= 2 ** 32
        req.host.ip = ipn
        req.host.port = port
        bodyStr = req.SerializeToString()
        reqStr = struct.pack('i', elb_pb2.ReportReqId) + struct.pack('i', len(bodyStr)) + bodyStr
        sock.sendto(reqStr, ('127.0.0.1', 8888 + i))

    def apiRegister(self, modid, cmdid):#非必需使用的API
        for i in range(3):
            ret, hostOrEmsg = self.apiGetHost(modid, cmdid, 50)
            if ret == 0:
                break
            time.sleep(0.05)
        return 0 if ret == 0 else -9998

    def __batchReportRes(self, cu):
        if cu.succCnt == 0:
            return
        modid, cmdid = cu.modid, cu.cmdid
        i = (modid + cmdid) % 3
        sock = self._socks[i]
        #create request
        req = elb_pb2.CacheBatchRptReq()
        req.modid = modid
        req.cmdid = cmdid
        for key in cu.succAccum:
            ip, port = key
            if cu.succAccum[key] == 0:
                continue
            #ip is str, but req.host.ip need big-endian number
            ipn = struct.unpack('I', socket.inet_aton(ip))[0]
            if ipn > 2 ** 31 - 1:
                ipn -= 2 ** 32
            h = req.results.add()
            h.ip = ipn
            h.port = port
            h.succCnt = cu.succAccum[key]
            #reset accumulator to 0
            cu.succAccum[key] = 0
        bodyStr = req.SerializeToString()
        reqStr = struct.pack('i', elb_pb2.CacheBatchRptReqId) + struct.pack('i', len(bodyStr)) + bodyStr
        sock.sendto(reqStr, ('127.0.0.1', 8888 + i))
        cu.succCnt = 0

    def __getRoute4Cache(self, modid, cmdid, ts):
        cacheItem = self.cache.get((modid, cmdid), None)
        req = elb_pb2.CacheGetRouteReq()
        req.modid, req.cmdid = modid, cmdid
        req.version = self.cache[(modid, cmdid)].version if cacheItem else -1
        rsp = elb_pb2.CacheGetRouteRsp()
        bodyStr = req.SerializeToString()
        reqStr = struct.pack('i', elb_pb2.CacheGetRouteReqId) + struct.pack('i', len(bodyStr)) + bodyStr
        i = (modid + cmdid) % 3
        sock = self._socks[i]
        #send
        try:
            sock.sendto(reqStr, ('127.0.0.1', 8888 + i))
        except socket.error, errMsg:
            print >> sys.stderr, errMsg
            return (-9999, errMsg)
        try:
            #recv response
            sock.settimeout(0.05)
            rspStr, addr = sock.recvfrom(65536)
            rspId = struct.unpack('i', rspStr[:4])[0]
            bodyLen = struct.unpack('i', rspStr[4:8])[0]
            if rspId != elb_pb2.CacheGetRouteRspId or bodyLen != len(rspStr[8:]):
                raise FormatError('message head format error')
            rsp.ParseFromString(rspStr[8:])
            if rsp.modid != modid or rsp.cmdid != cmdid:
                print >> sys.stderr, 'package content error'
                return (-9999, 'package content error')
            #已收到回复
            if rsp.version == -1:
                #remove cache if exist
                if cacheItem:
                    del self.cache[(modid, cmdid)]
            elif not cacheItem or cacheItem.version != rsp.version:
                #update route
                if not cacheItem:
                    cacheItem = CacheUnit()
                    cacheItem.modid = modid
                    cacheItem.cmdid = cmdid
                    self.cache[(modid, cmdid)] = cacheItem

                cacheItem.overload = rsp.overload
                cacheItem.version = rsp.version
                cacheItem.succCnt = 0
                cacheItem.lstUpdTs = ts
                cacheItem.nodeList = []
                cacheItem.succAccum = {}
                #添加路由信息
                for h in rsp.route:
                    ipn = h.ip
                    if ipn < 0:
                        ipn += 2 ** 32
                    ips = socket.inet_ntoa(struct.pack('I', ipn))
                    cacheItem.nodeList.append((ips, h.port))
                    cacheItem.succAccum[(ips, h.port)] = 0
            else:
                cacheItem.overload = rsp.overload
                cacheItem.succCnt = 0
                cacheItem.lstUpdTs = ts
        except socket.timeout:
            print >> sys.stderr, 'time out when recvfrom socket'
            return (-9999, 'time out when recvfrom socket')
        except FormatError as e:
            return (-9999, e.value)
        return (0, '')
