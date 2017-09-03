import random

class StaticRoute:
    def __init__(self):
        self.__route = None

    def getHost(self, modid, cmdid):
        if not self.__route:
            try:
                for i in range(3):
                    with open('/tmp/backupRoute.dat.%d' % (i + 1), 'r') as inf:
                        self.__route = {}
                        for line in inf:
                            attrs = line.strip().split()
                            imodid, icmdid = int(attrs[0]), int(attrs[1])
                            ip = attrs[2]
                            port = int(attrs[3])
                            key = (imodid, icmdid)
                            if (imodid, icmdid) not in self.__route:
                                self.__route[(imodid, icmdid)] = []
                            self.__route[(imodid, icmdid)].append((ip, port))

            except IOError, e:
                return -1
        if (modid, cmdid) not in self.__route:
            return -1
        host = random.choice(self.__route[(modid, cmdid)])
        return host

    def freeData(self):
        self.__route = None