#ifndef PROXY_H
#define PROXY_H

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <pthread.h>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>

class Request;
class Response;

using namespace std;

class Proxy {
public:
    Proxy(const char* port);
    ~Proxy();
    void start();


private:
    const char* port;
    // std::ofstream logFile;

    // static void* handle_request_helper(void* context);
    // void handle_request(int client_fd, User* user);
    static std::string getTime();
    static void * handle_request(void * arg);
    static void handle_GET(int client_fd, Request& request, int user_id);
    static void handle_POST(int client_fd, Request& request, int user_id);
    static void handle_CONNECT(int client_fd, Request& request, int user_id);
    // void log(const std::string& requestId, const std::string& message);
    int start_server(const char* port);
    static int create_client_socket(const char* host, const char* port);
    static bool findChunk(char * server_msg, int mes_len);
    static string constructConditionalRequest(const Request& originalRequest, const Response& cachedResponse);
    static void update_cache(const string&, const Response&);
    static string extract_chunk_status_line(const string& response);

};


#endif
