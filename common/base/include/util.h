#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

unsigned int murMurHash(const void *key, int len)
{
    const unsigned int m = 0x5bd1e995;
    const int r = 24;
    const int seed = 97;
    unsigned int h = seed ^ len;
    // Mix 4 bytes at a time into the hash
    const unsigned char *data = (const unsigned char *)key;
    while(len >= 4)
    {
        unsigned int k = *(unsigned int *)data;
        k *= m; 
        k ^= k >> r; 
        k *= m; 
        h *= m; 
        h ^= k;
        data += 4;
        len -= 4;
    }
    // Handle the last few bytes of the input array
    switch(len)
    {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0];
                h *= m;
    };
    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

#define HASHTO(kp, limit) murMurHash(kp, 8) % limit
/*
int main(int argc, char **argv)
{
    int modid = atoi(argv[1]);
    int cmdid = atoi(argv[2]);
    uint64_t key = ((uint64_t)modid << 32) + cmdid;
    unsigned int k = HASHTO(&key, 10);//murMurHash(&key, sizeof key);
    printf("murMurHash key = %u\n", k);
}
*/
#endif
