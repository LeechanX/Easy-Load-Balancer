import sys
from elbApi.elbClient import elbClient

if __name__ == '__main__':
    client = elbClient()
    modid = int(sys.argv[1])
    cmdid = int(sys.argv[2])
    ret, hostOrEmsg = client.apiGetHost(modid, cmdid, 10)
    if ret == 0:
        ip = hostOrEmsg[0]
        port = hostOrEmsg[1]
        print '%s:%d' % (ip, port)
        client.apiReportRes(modid, cmdid, ip, port, 0)
    elif ret == -9998:
        print '[%d,%d] not exist' % (modid, cmdid)
    else:
        print hostOrEmsg
