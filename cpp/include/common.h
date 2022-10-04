#pragma once
#include <assert.h>
#include <fcntl.h>
#include <linux/kernel-page-flags.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <signal.h>
#include <thread>
#include <pthread.h>
#include <mutex>
#include <condition_variable>

#define K (1024)
#define M (1024 * K)
#define G (1024 * M)
#define PAGE_SIZE 0x1000
#define PAGE_MASK (PAGE_SIZE-1)

#ifdef DEBUG
#define dbg_printf(...)      \
    do {                     \
        printf(__VA_ARGS__); \
        fflush(stdout);      \
    } while (0);
#else
#define dbg_printf(...)
#endif

#define MEASURE_TIME_COST_START(s) \
    do {                           \
        gettimeofday(&s, NULL);    \
    } while (0);

#define MEASURE_TIME_COST_END(logstr, s)                              \
    do {                                                              \
        struct timeval e;                                             \
        gettimeofday(&e, NULL);                                       \
        dbg_printf("[+] %s cost: %f sec\n", logstr, (e.tv_sec - s.tv_sec) + (e.tv_usec - s.tv_usec + 0.0) / 1e6); \
    } while (0);

#define MAP_HUGEPAGE_1G (30 << MAP_HUGE_SHIFT)
#define MAP_HUGEPAGE_2M (21 << MAP_HUGE_SHIFT)

#define NUM_DATA_PATTERN 12
#define NUM_HAMMER_METHOD 8
#define MAX_AGGRESSOR_ROW 32
#define MAX_AGGRESSOR_DIST 100
#define NUM_REPEAT 5

#ifdef USE_MULT_THREAD
#define NUM_THREADS 2
#endif

#define ROW_SIZE (8 * K)
#define SLICE_SZ (1 << 6)  // 6 is the smallest bank bit