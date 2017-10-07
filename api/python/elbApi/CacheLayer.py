class CacheUnit:
    def __init__(self):
        self.nodeList = []
        self.succAccum = {}
    def getHost(self):
        """return val [ip: string, port: int]"""
        host = self.nodeList.pop(0)
        self.nodeList.append(host)
        return host

    def report(self, ip, port):
        """ip: string, port: int"""
        if (ip, port) in self.succAccum:
            self.succAccum[(ip, port)] += 1
            self.succCnt += 1
