#ifndef REQUEST_H
#define REQUEST_H

#include <algorithm>
#include <iostream>
#include <istream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <string>

using namespace std;

class Request {
public:
    vector<char> request_info;
    string request_method;
    int server_port;
    string first_line;
    string host;
    string http_type;
    string request_target;
    string line;
    string url;
    // Check if the format is valid and it is one of the GET, POST or CONNECET functions
    // 1 for valid, 0 for invalid
    int is_valid = 1;

    Request(string request){
        extract_request(request);
        extract_method();
        get_first_line();
        extract_HTTP();
        extract_host();
        extract_target();
        extract_url();
    };

    ~Request(){};

    void extract_request(string request);
    void extract_method();
    string get_first_line();
    void extract_HTTP();
    void extract_host();
    // We use request.url as the key for cache map
    void extract_target();
    void extract_url();
};

#endif