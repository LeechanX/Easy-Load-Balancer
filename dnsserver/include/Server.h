#ifndef __DNSSERVER_H_
#define __DNSSERVER_H_

#include <stdint.h>
#include <set>
#include "easy_reactor.h"

extern tcp_server* server;

struct Interest
{
    std::set<uint64_t> m;
};

void pushChange(event_loop* loop, void* args);

#endif