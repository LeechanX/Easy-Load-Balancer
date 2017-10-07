#include "cacheLayer.h"
#include <arpa/inet.h>

void CacheUnit::getHost(std::string& ip, int& port)
{
    const pair<int, int> host = nodeList.front();
    port = host.second;
    //add to tail
    nodeList.pop_front();
    nodeList.push_back(host);
    struct in_addr saddr;
    saddr.s_addr = host.first;
    ip = ::inet_ntoa(saddr);
}

void CacheUnit::report(int ip, int port)
{
    uint64_t key = ((uint64_t)ip << 32) + port;
    if (succAccum.find(key) != succAccum.end())
    {
        succAccum[key] += 1;
        succCnt++;
    }
}

void CacheLayer::getAll(vector<CacheUnit*>& all)
{
    hash_map<uint64_t, CacheUnit*>::iterator it;
    for (it = _cache.begin();it != _cache.end(); ++it)
        all.push_back(it->second);
}

CacheUnit* CacheLayer::getCache(int modid, int cmdid)
{
    uint64_t key = ((uint64_t)modid << 32) + cmdid;
    CacheUnit* item = NULL;
    if (_cache.find(key) != _cache.end())
        item = _cache[key];
    return item;
}

void CacheLayer::addCache(int modid, int cmdid, CacheUnit* item)
{
    uint64_t key = ((uint64_t)modid << 32) + cmdid;
    _cache[key] = item;
}

void CacheLayer::rmCache(int modid, int cmdid)
{
    uint64_t key = ((uint64_t)modid << 32) + cmdid;
    CacheUnit* item = NULL;
    if (_cache.find(key) != _cache.end())
        item = _cache[key];
    if (item)
        delete item;
    _cache.erase(key);
}
