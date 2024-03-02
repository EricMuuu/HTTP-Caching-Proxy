#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <fstream>

extern pthread_mutex_t mutex;
extern std::ofstream proxyLog;

#endif
