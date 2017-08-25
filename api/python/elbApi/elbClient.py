import os
import mmap
import time
import socket
import struct
import elb_pb2
from StaticRoute import StaticRoute

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

    def __del__(self):
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
            staticData = self.__staticRoute.getHost(modid, cmdid)
            if staticData == -1:
                return (-10001, 'no data')
            return (0, staticData)

        self.__staticRoute.freeData()

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
            print errMsg
            return (-10003, errMsg)
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
                rspId = struct.unpack('i', rspStr[:4])
                bodyLen = struct.unpack('i', rspStr[4:8])
                if rspId != elb_pb2.GetHostReqId or bodyLen != len(rspStr[8:]):
                    raise FormatError('message head format error')
                rsp.ParseFromString(rspStr[8:])
            if rsp.seq != req.seq:
                return (-10001, 'request seq id is %d, response seq id is %d' % (req.seq, rsp.seq))
            elif rsp.retcode != 0:
                return (rsp.retcode, 'agent error')
            else:
                ipn = rsp.host.ip
                ips = socket.inet_ntoa(struct.pack('I', socket.htonl(ipn)))
                return (0, (ips, rsp.host.port))
        except socket.timeout:
            return (-10002, 'time out when recvfrom socket')
        except FormatError as e:
            return (-10001, e.value)

    def apiReportRes(self, modid, cmdid, ip, port, res):
        i = (modid + cmdid) % 3
        sock = self._socks[i]
        #create request
        req = elb_pb2.ReportReq()
        req.modid = modid
        req.cmdid = cmdid
        req.retcode = res
        req.host.ip = ip
        req.host.port = port
        bodyStr = req.SerializeToString()
        reqStr = struct.pack('i', elb_pb2.ReportReqId) + struct.pack('i', len(bodyStr)) + bodyStr
        sock.sendto(reqStr, ('127.0.0.1', 8888 + i))

if __name__ == '__main__':
    client = elbClient()
    ret, hostOrEmsg = client.apiGetHost(10001, 1001, 10)
    if ret == 0:
        print hostOrEmsg
    else:
        print hostOrEmsg
    client.apiReportRes(10001, 1001, 1, 2, 3)