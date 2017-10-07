#ifndef __CACHELAYER_H__
#define __CACHELAYER_H__

#include <list>
#include <string>
#include <stdint.h>
#include <ext/hash_map>

using std::pair;
using std::list;
using std::vector;
using std::string;
using __gnu_cxx::hash_map;

//cache unit
struct CacheUnit
{
    CacheUnit() { }
    //get host
    void getHost(string& ip, int& port);
    //report 0 in cache for host[ip:port]
    void report(int ip, int port);

    int modid;
    int cmdid;
    bool overload;
    long lstUpdTs;
    long version;
    uint64_t succCnt;

    list<pair<int, int> > nodeList;
    //key => success accumulator
    hash_map<uint64_t, uint32_t> succAccum;
};

struct CacheLayer
{
public:
    CacheUnit* getCache(int modid, int cmdid);

    void addCache(int modid, int cmdid, CacheUnit* item);

    void rmCache(int modid, int cmdid);

    void getAll(vector<CacheUnit*>& all);

private:
    hash_map<uint64_t, CacheUnit*> _cache;
};

#endif
