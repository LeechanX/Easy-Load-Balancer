#ifndef __ALIVE_DO_HEADER__
#define __ALIVE_DO_HEADER__

#define DEAD_THRESHOLD 2

#define HB_FILE "/tmp/hb_map.bin"

#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

class HeartBeat
{
public:
    HeartBeat(bool create = false)
    {
        int fd = open(HB_FILE, O_CREAT | O_EXCL | O_RDWR, 0666);
        if (fd == -1 && errno != EEXIST)
        {
            perror("open /tmp/hb_map.bin");
            exit(1);
        }
        if (fd == -1)
            fd = open(HB_FILE, O_RDWR);

        if(ftruncate(fd, 8) == -1)
        {
            perror("ftruncate /tmp/hb_map.bin");
            exit(1);
        }
        _hb = (uint64_t *)mmap(0, 8, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (!_hb)
        {
            perror("mmap /tmp/hb_map.bin");
            exit(1);
        }
        if (create)
            *_hb = time(NULL);
    }

    bool die() const { return time(NULL) - *_hb > DEAD_THRESHOLD; }

    void recordTs() { *_hb = time(NULL); }

private:
    uint64_t *_hb;
};

#endif
