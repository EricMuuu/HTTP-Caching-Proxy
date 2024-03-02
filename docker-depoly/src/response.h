#ifndef RESPONSE_H
#define RESPONSE_H

#include <algorithm>
#include <iostream>
#include <istream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <string>

using namespace std;

class Response {
public:
    vector<char> response_info;
    vector<char> response_content;
    int user_id;
    string status_line;
    string status_code;
    string cache_control;
    string date;
    // This is the max-age field in cache-control
    string age;
    string expire;
    string etag;
    string last_modified;

    Response(string response, int user_id) {
        extract_response(response);
        extract_status_line();
        extract_status_code();
        extract_cache_control();
        extract_date();
        extract_age();
        extract_expire();
        extract_etag();
        extract_modify();
        // extract_content();
    };

    ~Response(){};

    // Check if it is a valid response
    // 1 for valid, 0 for invalid
    int is_valid = 1;

    void extract_response(string response);
    void extract_status_line();
    void extract_status_code();
    void extract_cache_control();

    // For these 5 functions check if they are optional to write cerr messages
    void extract_date();
    void extract_age();
    void extract_expire();
    void extract_etag();
    void extract_modify();
    // void extract_content();
    // As the key of cache
    // void generate_key();
    string cache_response(const string& requestUrl, int user_id);
};

#endif