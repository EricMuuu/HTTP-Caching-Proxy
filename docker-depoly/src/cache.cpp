#include <unordered_map>
#include <string>
#include <chrono>
#include <iostream>
#include <sstream>
#include <optional>

#include "response.h"
#include "cache.h"
#include "common.h"

using namespace std;
using chrono::system_clock;

unordered_map<string, CacheEntry> cache;

CacheEntry::CacheEntry() : response(nullptr) {}

CacheEntry::CacheEntry(Response* resp, std::chrono::system_clock::time_point time) 
    : response(resp), cached_time(time) {}

CacheEntry::~CacheEntry() {
    delete response;
}

CacheEntry::CacheEntry(CacheEntry&& other) noexcept 
    : response(other.response), cached_time(other.cached_time) {
    other.response = nullptr;
}

CacheEntry& CacheEntry::operator=(CacheEntry&& other) noexcept {
    if (this != &other) {
        delete response;
        response = other.response;
        other.response = nullptr;
        cached_time = other.cached_time;
    }
    return *this;
}

size_t cur_size = 0;
// 500 MB
const size_t max_size = 500 * 1024 * 1024;

// Replacement strategy: When cache is full, we remove the cached responses starting from the one that has the earliest cached time
void removeOldestEntry() {
    auto oldest = cache.begin();
    for (auto it = cache.begin(); it != cache.end(); ++it) {
        if (it->second.cached_time < oldest->second.cached_time) {
            oldest = it;
        }
    }
    if (oldest != cache.end()) {
        cur_size -= estimateSize(*(oldest->second.response));
        cache.erase(oldest);
    }
}

size_t estimateSize(const Response& response) {
    return sizeof(Response) + response.response_info.size();
}

chrono::system_clock::time_point parseExpireString(const string& expire) {
    tm tm = {};
    istringstream ss(expire);
    auto tp = chrono::system_clock::from_time_t(mktime(&tm));
    return tp;
}

void add_to_cache(const string& key, Response& response, int user_id) {
    size_t response_size = estimateSize(response);
    while (cur_size + response_size > max_size && !cache.empty()) {
        removeOldestEntry();
    }
    if (cur_size + response_size <= max_size) {
        Response* newResponse = new Response(response);
        cache[key] = CacheEntry(newResponse, system_clock::now());
        cur_size += response_size;
    } else {
        pthread_mutex_lock(&mutex);
        proxyLog << user_id << ": WARNING - Failed to add response to cache due to size limit." << endl;
        cout << user_id << ": WARNING - Failed to add response to cache due to size limit." << endl;
        pthread_mutex_unlock(&mutex);
    }
}

int parseAge(const string& age) {
    return stoi(age);
}

string expirationTimeToString(const chrono::system_clock::time_point& timePoint) {
    auto timeT = chrono::system_clock::to_time_t(timePoint);
    tm* tm = gmtime(&timeT);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", tm);
    return string(buffer);
}

chrono::system_clock::time_point calculateExpirationTime(const CacheEntry& entry) {
    // We prioritize max-age field 
    if (!entry.response->age.empty()) {
        int ageInSeconds = parseAge(entry.response->age);
        return entry.cached_time + chrono::seconds(ageInSeconds);
    } else if (!entry.response->expire.empty()) {
        return parseExpireString(entry.response->expire);
    }
    throw runtime_error("Cannot get expiration time");
}

bool isExpired(const CacheEntry& entry) {
    if (entry.response->age.empty() && entry.response->expire.empty()){
        return false;
    }
    auto now = system_clock::now();
    auto expiryTime = calculateExpirationTime(entry);
    return now > expiryTime;
}

bool requiresValidation(const Response& response) {
    return response.cache_control.find("no-cache") != string::npos ||
           response.cache_control.find("must-revalidate") != string::npos ||
           !response.etag.empty() || !response.last_modified.empty();
}


optional<Response> get_from_cache(const string& key, int user_id) {
    auto it = cache.find(key);

    if (it == cache.end()) {
        pthread_mutex_lock(&mutex);
        proxyLog << user_id << ": not in cache" << endl;
        cout << user_id << ": not in cache" << endl;
        pthread_mutex_unlock(&mutex);
        return nullopt;
    }
    bool validate = requiresValidation(*(it->second.response));
    // If need to revalidate, we still return the response, but we will first validate in proxy.cpp before serving
    if (validate) {
        pthread_mutex_lock(&mutex);
        cout << user_id << ": in cache, requires validation" << endl;
        proxyLog << user_id << ": in cache, requires validation" << endl;
        pthread_mutex_unlock(&mutex);
        return *(it->second.response);
    } else if (isExpired(it->second)) {
        auto expiryTime = calculateExpirationTime(it->second);
        pthread_mutex_lock(&mutex);
        cout << user_id << ": in cache, but expired at " << expirationTimeToString(expiryTime) << endl;
        proxyLog << user_id << ": in cache, but expired at " << expirationTimeToString(expiryTime) << endl;
        pthread_mutex_unlock(&mutex);
        cur_size -= estimateSize(*(it->second.response));
        cache.erase(it);
        return nullopt;
    } else {
        pthread_mutex_lock(&mutex);
        proxyLog << user_id << ": in cache, valid" << endl;
        cout << user_id << ": in cache, valid" << endl;
        pthread_mutex_unlock(&mutex);
    }
    return *(it->second.response);
}


