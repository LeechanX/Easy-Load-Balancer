from elbApi.elbClient import elbClient

if __name__ == '__main__':
    client = elbClient()
    ret, hostOrEmsg = client.apiGetHost(10001, 1001, 10)
    if ret == 0:
        print hostOrEmsg
    else:
        print hostOrEmsg
    client.apiReportRes(10001, 1001, 1, 2, 3)