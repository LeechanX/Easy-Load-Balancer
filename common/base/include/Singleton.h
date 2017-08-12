#ifndef __SINGLETON_H__
#define __SINGLETON_H__

#include <assert.h>
#include <pthread.h>

template<typename T>
class Singleton
{
public:
    static void init()
    {
        _ins = new T();
        assert(_ins);
    }

    static T* ins()
    {
        pthread_once(&_once, init);
        return _ins;
    }

private:
    Singleton(const Singleton&);//necessary
    const Singleton& operator=(const Singleton&);//necessary

    static T* _ins;
    static pthread_once_t _once;
};

template<typename T>
T* Singleton<T>::_ins = NULL;

template<typename T>
pthread_once_t Singleton<T>::_once = PTHREAD_ONCE_INIT;

#endif
