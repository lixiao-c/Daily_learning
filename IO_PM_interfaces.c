#define _GNU_SOURCE

#include <linux/kernel.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <immintrin.h>
#include <stdio.h>
#include <sys/time.h>

#define TIMES 10000
struct timeval t1, t2;

unsigned long long td( struct timeval *t1, struct timeval *t2 )
{
    unsigned long long dt = t2->tv_sec * 1000000 + t2->tv_usec;
    return dt - t1->tv_sec * 1000000 - t1->tv_usec;
}
void stime()
{
    gettimeofday(&t1, NULL);
}

unsigned long long etime(char *s)
{
    gettimeofday(&t2, NULL);
    unsigned long long dt = t2.tv_sec * 1000000 + t2.tv_usec;
    unsigned long long ret = dt - t1.tv_sec * 1000000 - t1.tv_usec;
    printf("%s %llu us\n", s, ret);
    return ret;
}


/* MMap nvm region. */
static void *
mapFile(const char * pathname, // File name.
        int initSize, // Initial file size.
        bool *isNew) // Is it a new file? [OUT].
{
    *isNew = false;
    void *addr;
    int err;
    int fd;

    if (access(pathname, F_OK) != 0) {
        if ((fd = open(pathname, O_RDWR | O_CREAT | O_EXCL,
                        S_IRUSR | S_IWUSR)) < 0)
        {
            printf("create file on NVM error\n");
            return NULL;
        }
        printf("First time file creation\n");
        *isNew = true;

        if ((err = posix_fallocate(fd, 0, initSize)) != 0) {
            printf("failed to allocate space\n");
            close(fd);
            return NULL;
        }

        if ((err = fsync(fd)) != 0) {
            printf("fsync failed\n");
            close(fd);
            return NULL;
        }
    } else {
        if ((fd = open(pathname, O_RDWR|O_DIRECT|O_SYNC)) < 0) {
            printf("open file on NVM error\n");
            return NULL;
        }
        printf("File already exists on NVM\n");
        *isNew = false;
    }

    if ((addr = mmap(NULL, initSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, 0)) == MAP_FAILED)
    {
        printf("failed to mmap\n");
        close(fd);
        return NULL;
    }
    close(fd);
    return addr;
}

/* just open nvm region and return fd. */
int
openFile(const char * pathname, // File name.
        int initSize, // Initial file size.
        bool *isNew) // Is it a new file? [OUT].
{
    *isNew = false;
    void *addr;
    int err;
    int fd;

    if (access(pathname, F_OK) != 0) {
        if ((fd = open(pathname, O_RDWR | O_CREAT | O_EXCL,
                        S_IRUSR | S_IWUSR)) < 0)
        {
            printf("create file on NVM error\n");
            return NULL;
        }
        printf("First time file creation\n");
        *isNew = true;

        if ((err = posix_fallocate(fd, 0, initSize)) != 0) {
            printf("failed to allocate space\n");
            close(fd);
            return NULL;
        }

        if ((err = fsync(fd)) != 0) {
            printf("fsync failed\n");
            close(fd);
            return NULL;
        }
    } else {
        if ((fd = open(pathname, O_RDWR)) < 0) {
            printf("open file on NVM error\n");
            return NULL;
        }
        printf("File already exists on NVM\n");
        *isNew = false;
    }
/*
    if ((addr = mmap(NULL, initSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                    fd, 0)) == MAP_FAILED)
    {
        printf("failed to mmap\n");
        close(fd);
        return NULL;
    }
    close(fd);
*/
    return fd;
}


int main(int argc, char **argv) {
    int i;
    bool pmemnew;

    char *pmemaddr = mapFile("/pmem0/testflush", 1024, &pmemnew);
    //unsigned char *pmemaddr = malloc(sizeof(unsigned char) * 1024);
    int fd_pm = openFile("/pmem0/testflush_io", 1024, &pmemnew);
    if (!fd_pm) {
        printf("no fd_pm now\n");
        return 0;
    }

    if (pmemaddr == NULL) {
        printf("File mapping failed\n");
        exit(1);
    }
    // write to pmem
	stime();
    for (i = 0; i < TIMES; i++) {
        *(pmemaddr) = i%250;
        //_mm_clflush(pmemaddr);
        //_mm_sfence();
    }
    etime("PM: no sync\n");

	stime();
    for (i = 0; i < TIMES; i++) {
        *(pmemaddr) = i%250;
        msync(pmemaddr, sizeof(int), MS_SYNC);
    }
    etime("PM: with msync\n");

	stime();
    for (i = 0; i < TIMES; i++) {
        *(pmemaddr) = i%250;
        _mm_clflush(pmemaddr);
        _mm_sfence();
    }
    etime("PM: with clflush and sfence\n");

    int buf;
    printf("sizeof(int): %d\n", sizeof(int));
	stime();
    for (i = 0; i < TIMES; i++) {
        buf = i%250;
        //pwrite(fd_pm, &buf, sizeof(int), 0);
        pwrite(fd_pm, &buf, sizeof(int), 0);
        //_mm_clflush(pmemaddr);
        //_mm_sfence();
    }
    etime("PM_IO: O_DIRECT\n");


    //long int ret_status = syscall(333); // 333 is a newly added syscall that causes kernel panic
    //printf("syscall ret status = %ld", ret_status);
}
