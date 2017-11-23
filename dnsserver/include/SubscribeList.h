#ifndef __SUBSCRIBELIST_H__
#define __SUBSCRIBELIST_H__

#include <stdint.h>
#include <vector>
#include <pthread.h>
#include <ext/hash_set>
#include <ext/hash_map>

class SubscribeList
{
public:
    void subscribe(uint64_t mod, int fd);

    void unsubscribe(uint64_t mod, int fd);

    //将有变更的mod加入待push队列
    void push(std::vector<uint64_t>& changes);

    void fetchPush(__gnu_cxx::hash_set<int>& subscribers,
        __gnu_cxx::hash_map<int, __gnu_cxx::hash_set<uint64_t> >& news);
private:
    //记录订阅信息: mod -> fds
    __gnu_cxx::hash_map<uint64_t, __gnu_cxx::hash_set<int> > bookl;
    pthread_mutex_t booklock;
    //记录已产生待push的消息: fd -> mods
    __gnu_cxx::hash_map<int, __gnu_cxx::hash_set<uint64_t> > pushl;
    pthread_mutex_t pushlock;
};

#endif