import socket
import struct
import elb_pb2

class elbClient:
    def __init__(self):
        self.socks = [None for i in range(3)]
        try:
            self.socks[0] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socks[1] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socks[2] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socks[0].bind(('127.0.0.1', 8888))
            self.socks[1].bind(('127.0.0.1', 8889))
            self.socks[2].bind(('127.0.0.1', 8890))
        except socket.error, errMsg:
            print errMsg
            exit(1)
        self.seq = 0

    def __del__(self):
        for sock in self.socks:
            sock.close()

    def apiGetHost(self, modid, cmdid, timo):
        if timo < 10: timo = 10
        if timo > 1000: timo = 1000
        i = (modid + cmdid) % 3
        sock = self.socks[i]
        if self.seq == 2 ** 31: self.seq = 0
        #create request
        req = elb_pb2.GetHostReq()
        req.seq = self.seq
        self.seq += 1
        req.modid = modid
        req.cmdid = cmdid
        rsp = elb_pb2.GetHostRsp()
        #send request
        try:
            sock.sendto(req.SerializeToString(), ('127.0.0.1', 8888 + i))
        except socket.error, errMsg:
            print errMsg
            return -10003
        try:
            #recv response
            sock.settimeout(timo)
            rspStr, addr = sock.recvfrom(65536)
            rsp.ParseFromString(rspStr)
            while rsp.seq < req.seq:
                rspStr, addr = sock.recvfrom(65536)
                rsp.ParseFromString(rspStr)
            if rsp.seq != req.seq:
                print 'request seq id is %d, response seq id is %d' % (req.seq, rsp.seq)
                return (-10001, None)
            elif rsp.retcode != 0:
                return (rsp.retcode, None)
            else:
                ipn = rsp.host.ip
                ips = socket.inet_ntoa(struct.pack('I', socket.htonl(ipn)))
                return (0, (ips, rsp.host.port))
        except socket.timeout:
            print 'time out when recvfrom socket'
            return (-10002, None)

    def apiReportRes(self, modid, cmdid, ip, port, res):
        i = (modid + cmdid) % 3
        sock = self.socks[i]
        #create request
        req = elb_pb2.ReportReq()
        req.modid = modid
        req.cmdid = cmdid
        req.retcode = res
        #req.host.ip = ip
        req.host.port = port
        sock.send(req.SerializeToString())

if __name__ == '__main__':
    client = elbClient()
    print client.apiGetHost(10001, 1001, 10)