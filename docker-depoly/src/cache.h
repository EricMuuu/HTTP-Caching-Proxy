#ifndef CACHE_H
#define CACHE_H

#include <unordered_map>
#include <string>
#include <chrono>
#include <optional>
#include "response.h"


using namespace std;

class CacheEntry {
public:
    Response* response;
    chrono::system_clock::time_point cached_time;

    CacheEntry();
    CacheEntry(Response* resp, chrono::system_clock::time_point time);
    ~CacheEntry();
    CacheEntry(const CacheEntry&) = delete;
    CacheEntry& operator=(const CacheEntry&) = delete;
    CacheEntry(CacheEntry&& other) noexcept;
    CacheEntry& operator=(CacheEntry&& other) noexcept;
};

extern unordered_map<string, CacheEntry> cache;
extern size_t cur_size;
extern const size_t max_size;

optional<Response> get_from_cache(const string& key, int user_id);
void add_to_cache(const string& key, Response& response, int user_id);
string expirationTimeToString(const chrono::system_clock::time_point& timePoint);
chrono::system_clock::time_point calculateExpirationTime(const CacheEntry& entry);
bool isExpired(const CacheEntry& entry);
bool requiresValidation(const Response& response);
void removeOldestEntry();
size_t estimateSize(const Response& response);

#endif
