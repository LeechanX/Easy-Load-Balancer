#ifndef __ROUTE_H__
#define __ROUTE_H__

#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <ext/hash_map>
#include <ext/hash_set>
#include "mysql.h"
#include "Singleton.h"

using __gnu_cxx::hash_set;
using __gnu_cxx::hash_map;

typedef hash_map<uint64_t, hash_set<uint64_t> > routeMap;
typedef hash_map<uint64_t, hash_set<uint64_t> >::iterator routeMapIt;
typedef hash_set<uint64_t> hostSet;
typedef hash_set<uint64_t>::iterator hostSetIt;

class Route
{
public:
    //main threads call it
    hostSet getHosts(int modid, int cmdid);

    //backend thread call it
    int reload();

    //backend thread call it
    void swap();

    long routeVersion;

    int loadVersion();

    void loadChanges(std::vector<uint64_t>& changes);

    void rmChanges(bool recent = true);

private:
    Route();

    friend class Singleton<Route>;

    //~Route(); No need to write ~Route()

    MYSQL _dbConn;
    pthread_rwlock_t _rwlock;
    routeMap* _data;
    routeMap* _tmpData;

    char _sql[1000];
};

//backend thread domain
void* dataLoader(void* args);

#endif
