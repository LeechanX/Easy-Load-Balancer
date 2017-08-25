#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "easy_reactor.h"
#include "elbApi.h"
#include "elb.pb.h"
#include "HeartBeat.h"

elbClient::elbClient(): _seqid(0)
{
    struct sockaddr_in servaddr;
    ::bzero(&servaddr, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    ::inet_aton("127.0.0.1", &servaddr.sin_addr);

    for (int i = 0;i < 3; ++i)
    {
        _sockfd[i] = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
        if (_sockfd[i] == -1)
        {
            perror("socket()");
            ::exit(1);
        }
        servaddr.sin_port = htons(8888 + i);
        int ret = ::connect(_sockfd[i], (const struct sockaddr*)&servaddr, sizeof servaddr);
        if (ret == -1)
        {
            perror("connect()");
            ::exit(1);
        }
    }
    _hb = new HeartBeat();
    if (!_hb)
    {
        fprintf(stderr, "no space to new HeartBeat\n");
        ::exit(1);
    }
}

int elbClient::apiGetHost(int modid, int cmdid, int timo, std::string& ip, int& port)
{
    if (((HeartBeat*)_hb)->die())
    {
        int ret = _staticRoute.getHost(modid, cmdid, ip, port);
        if (ret == -1)
        {
            return -100011;
        }
        return 0;
    }
    _staticRoute.freeData();

    uint32_t seq = _seqid++;
    elb::GetHostReq req;
    req.set_seq(seq);
    req.set_modid(modid);
    req.set_cmdid(cmdid);
    //send
    char wbuf[4096], rbuf[4096];
    commu_head head;
    head.length = req.ByteSize();
    head.cmdid = elb::GetHostReqId;
    ::memcpy(wbuf, &head, COMMU_HEAD_LENGTH);
    req.SerializeToArray(wbuf + COMMU_HEAD_LENGTH, head.length);
    int index = (modid + cmdid) % 3;
    int ret = ::sendto(_sockfd[index], wbuf, head.length + COMMU_HEAD_LENGTH , 0, NULL, 0);
    if (ret == -1)
    {
        perror("sendto");
        return -10001;
    }

    struct timeval tv;
    tv.tv_sec = timo / 1000;
    tv.tv_usec = (timo % 1000) * 1000;
    ::setsockopt(_sockfd[index], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int pkgLen = ::recvfrom(_sockfd[index], rbuf, sizeof rbuf, 0, NULL, NULL);
    if (pkgLen == -1)
    {
        perror("recvfrom");
        return -10001;
    }

    ::memcpy(&head, rbuf, COMMU_HEAD_LENGTH);
    elb::GetHostRsp rsp;
    if (head.cmdid != elb::GetHostRspId || 
        !rsp.ParseFromArray(rbuf + COMMU_HEAD_LENGTH, pkgLen - COMMU_HEAD_LENGTH))
    {
        return -10002;
    }
    while (rsp.seq() < seq)
    {
        //recv again
        pkgLen = ::recvfrom(_sockfd[index], rbuf, sizeof rbuf, 0, NULL, NULL);
        if (pkgLen == -1)
        {
            perror("recvfrom");
            return -10001;
        }
        ::memcpy(&head, rbuf, COMMU_HEAD_LENGTH);
        if (head.cmdid != elb::GetHostRspId || 
            !rsp.ParseFromArray(rbuf + COMMU_HEAD_LENGTH, pkgLen - COMMU_HEAD_LENGTH))
        {
            return -10002;
        }
    }
    if (rsp.seq() != seq || rsp.modid() != modid || rsp.cmdid() != cmdid)
    {
        return -10002;
    }

    if (rsp.retcode() == 0)
    {
        const elb::HostAddr host = rsp.host();
        struct in_addr inaddr;
        inaddr.s_addr = host.ip();
        ip = ::inet_ntoa(inaddr);
        port = host.port();
    }
    return rsp.retcode();
}

void elbClient::apiReportRes(int modid, int cmdid, const std::string& ip, int port, int retcode)
{
    struct in_addr inaddr;
    ::inet_aton(ip.c_str(), &inaddr);

    elb::ReportReq req;
    req.set_modid(modid);
    req.set_cmdid(cmdid);
    req.set_retcode(retcode);

    elb::HostAddr* hp = req.mutable_host();
    hp->set_ip(inaddr.s_addr);
    hp->set_port(port);

    //send
    char wbuf[4096];
    commu_head head;
    head.length = req.ByteSize();
    head.cmdid = elb::ReportReqId;
    ::memcpy(wbuf, &head, COMMU_HEAD_LENGTH);
    req.SerializeToArray(wbuf + COMMU_HEAD_LENGTH, head.length);

    int index = (modid + cmdid) % 3;
    int ret = ::sendto(_sockfd[index], wbuf, head.length + COMMU_HEAD_LENGTH , 0, NULL, 0);
    if (ret == -1)
        perror("sendto()");
}
