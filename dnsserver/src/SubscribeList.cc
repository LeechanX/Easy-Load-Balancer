#include "SubscribeList.h"
#include "Server.h"

void SubscribeList::subscribe(uint64_t mod, int fd)
{
    ::pthread_mutex_lock(&booklock);
    bookl[mod].insert(fd);
    ::pthread_mutex_unlock(&booklock);
}

void SubscribeList::unsubscribe(uint64_t mod, int fd)
{
    ::pthread_mutex_lock(&booklock);
    if (bookl.find(mod) != bookl.end())
    {
        bookl[mod].erase(fd);
        if (bookl[mod].empty())
        {
            bookl.erase(mod);
        }
    }
    ::pthread_mutex_unlock(&booklock);
}

//将有变更的mod加入待push队列, 并通知各个线程
void SubscribeList::push(std::vector<uint64_t>& changes)
{
    std::vector<uint64_t>::iterator it;
    __gnu_cxx::hash_set<int>::iterator st;

    ::pthread_mutex_lock(&booklock);
    ::pthread_mutex_lock(&pushlock);
    for (it = changes.begin();it != changes.end(); ++it)
    {
        uint64_t mod = *it;
        if (bookl.find(mod) != bookl.end())
        {
            for (st = bookl[mod].begin();st != bookl[mod].end(); ++st)
            {
                int fd = *st;
                pushl[fd].insert(mod);
            }
        }
    }
    ::pthread_mutex_unlock(&pushlock);
    ::pthread_mutex_unlock(&booklock);

    //通知各个线程都去执行pushChange
    server->threadPool()->run_task(pushChange);
}

void SubscribeList::fetchPush(__gnu_cxx::hash_set<int>& subscribers,
    __gnu_cxx::hash_map<int, __gnu_cxx::hash_set<uint64_t> >& news)
{
    __gnu_cxx::hash_map<int, __gnu_cxx::hash_set<uint64_t> >::iterator it;
    ::pthread_mutex_lock(&pushlock);
    for (it = pushl.begin();it != pushl.end();)
    {
        if (subscribers.find(it->first) != subscribers.end())
        {
            news[it->first] = pushl[it->first];
            pushl.erase(it++);
        }
        else
        {
            ++it;
        }
    }
    ::pthread_mutex_unlock(&pushlock);
}
