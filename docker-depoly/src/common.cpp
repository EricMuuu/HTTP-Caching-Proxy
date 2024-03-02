#include "common.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
std::ofstream proxyLog("/var/log/erss/proxy.log");
