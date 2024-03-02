#include "proxy.h"
#include "request.h"
#include "response.h"
#include "user.h"
#include "cache.h"
#include "common.h"

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <ctime>
#include <chrono>

using namespace std;

// Constructor
Proxy::Proxy(const char* port) : port(port) {
}

// Destructor
Proxy::~Proxy() {
}

int Proxy::start_server(const char* port) {
    struct addrinfo hints, *res, *p;
    int sockfd, yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &res) != 0) {
        pthread_mutex_lock(&mutex);
        proxyLog << "(no-id): ERROR - getaddrinfo error" << endl;
        cerr << "(no-id): ERROR - getaddrinfo error" << endl;
        pthread_mutex_unlock(&mutex);
        
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            pthread_mutex_lock(&mutex);
            proxyLog << "(no-id): ERROR - socket" << endl;
            cerr << "(no-id): ERROR - socket" << endl;
            pthread_mutex_unlock(&mutex);
            
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            pthread_mutex_lock(&mutex);
            proxyLog << "(no-id): ERROR - setsockopt" << endl;
            cerr << "(no-id): ERROR - setsockopt" << endl;
            pthread_mutex_unlock(&mutex);
            close(sockfd);
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            pthread_mutex_lock(&mutex);
            proxyLog << "(no-id): ERROR - bind" << endl;
            cerr << "(no-id): ERROR - bind" << endl;
            pthread_mutex_unlock(&mutex);
            close(sockfd);
            continue;
        }
        break;
    }

    freeaddrinfo(res);

    if (p == NULL) {
        pthread_mutex_lock(&mutex);
        proxyLog << "(no-id): ERROR - failed to bind" << endl;
        cerr << "(no-id): ERROR - failed to bind" << endl;
        pthread_mutex_unlock(&mutex);
        
        return -1;
    }

    if (listen(sockfd, 10) == -1) {
        pthread_mutex_lock(&mutex);
        proxyLog << "(no-id): ERROR - listen" << endl;
        cerr << "(no-id): ERROR - listen" << endl;
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    return sockfd;
}

int Proxy::create_client_socket(const char* host, const char* port) {
    struct addrinfo hints, *res, *p;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        pthread_mutex_lock(&mutex);
        proxyLog << "(no-id): ERROR - getaddrinfo error" << endl;
        cerr << "(no-id): ERROR - getaddrinfo error" << endl;
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            pthread_mutex_lock(&mutex);
            proxyLog << "(no-id): ERROR - socket" << endl;
            cerr << "(no-id): ERROR - socket" << endl;
            pthread_mutex_unlock(&mutex);
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            pthread_mutex_lock(&mutex);
            proxyLog << "(no-id): ERROR - connect" << endl;
            cerr << "(no-id): ERROR - connect" << endl;
            pthread_mutex_unlock(&mutex);
            continue;
        }
        break;
    }

    freeaddrinfo(res);

    if (p == NULL) {
        pthread_mutex_lock(&mutex);
        proxyLog << "(no-id): ERROR - failed to connect" << endl;
        cerr << "(no-id): ERROR - failed to connect" << endl;
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    return sockfd;
}



void* Proxy::handle_request(void* info) {
    User* user = (User*)info;
    int client_fd = user->get_fd();

    vector<char> buffer(4096);
    ssize_t bytes_received = recv(client_fd, buffer.data(), buffer.size(), 0);
    if (bytes_received < 0) {
        pthread_mutex_lock(&mutex);
        proxyLog << user->get_id() << ": Error reading request from client" << endl;
        cerr << user->get_id() << "Error reading request from client" << endl;
        pthread_mutex_unlock(&mutex);
        return nullptr;
    }

    if (bytes_received == 0) {
        close(client_fd);
        return nullptr;
    }

    string data(buffer.begin(), buffer.begin() + bytes_received);
    Request request(data);
    if (request.request_method != "GET" && request.request_method != "POST" && request.request_method != "CONNECT") {
        pthread_mutex_lock(&mutex);
        proxyLog << user->get_id() << ": Responding \"HTTP/1.1 400 Bad Request\"" << endl;
        cerr << user->get_id() << ": Responding \"HTTP/1.1 400 Bad Request\"" << endl;
        pthread_mutex_unlock(&mutex);
        return nullptr;
    }

    pthread_mutex_lock(&mutex);
    proxyLog << user->get_id() << ": \"" << request.get_first_line() << "\" from "
            << user->get_ip() << " @ " << Proxy::getTime() << endl;
    cout << user->get_id() << ": \"" << request.get_first_line() << "\" from "
            << user->get_ip() << " @ " << Proxy::getTime() << endl;
    pthread_mutex_unlock(&mutex);

    // see full request
    // cout.write(buffer.data(), bytes_received);

    if (request.request_method == "GET") {
        handle_GET(client_fd, request, user->get_id());
    } else if (request.request_method == "POST") {
        handle_POST(client_fd, request, user->get_id());
    } else if (request.request_method == "CONNECT") {
        handle_CONNECT(client_fd, request, user->get_id());
        pthread_mutex_lock(&mutex);
        proxyLog << user->get_id() << ": Tunnel closed" << endl;
        cout << user->get_id() << ": Tunnel closed" << endl;
        pthread_mutex_unlock(&mutex);
        
    }

    close (client_fd);
    return nullptr;
}



void Proxy::start() {

    int sockfd = start_server(port);
    if (sockfd == -1) {
        cerr << "Failed to start server" << endl;
        return;
    }
    int client_fd;
    int id = 0;
    while (true) {
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof(client_addr);
        client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) {
            cerr << "Failed to accept connection" << endl;
            continue;
        }

        struct sockaddr_in * addr = (struct sockaddr_in *)&client_addr;
        string ip = inet_ntoa(addr->sin_addr);

        pthread_t thread_id;

        pthread_mutex_lock(&mutex);
        User *user = new User();
        user->set_id(id);
        user->set_fd(client_fd); 
        user->set_ip(ip);
        id++;
        pthread_mutex_unlock(&mutex);
        
        pthread_create(&thread_id, NULL, handle_request, user);
        
        pthread_detach(thread_id);
    }

    close(sockfd);
}

void Proxy::handle_GET(int client_fd, Request& request, int user_id) {
    auto cacheResponseTemp = get_from_cache(request.url, user_id);
    //If in cache
    if (cacheResponseTemp.has_value()) {
        Response& cacheResponse = cacheResponseTemp.value();
        
        //If validation is needed
        if (requiresValidation(cacheResponse)) {
            string conditionalRequestStr = constructConditionalRequest(request, cacheResponse);
            int server_fd = create_client_socket(request.host.c_str(), "80");
            if (server_fd == -1) {
                pthread_mutex_lock(&mutex);
                cerr << user_id << ": ERROR -Failed to connect to server" << endl;
                proxyLog << user_id << ": ERROR - Failed to connect to server" << endl;
                pthread_mutex_unlock(&mutex);
                close(client_fd);
                return;
            }

            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": NOTE - Validating from " << request.host << endl;
            cout << user_id << ": NOTE - Validating from " << request.host << endl;
            pthread_mutex_unlock(&mutex);

            if (send(server_fd, conditionalRequestStr.c_str(), conditionalRequestStr.size(), 0) < 0) {
                pthread_mutex_lock(&mutex);
                cerr << user_id << ": ERROR - Failed to send conditional request to server" << endl;
                proxyLog << user_id << ": ERROR - Failed to send conditional request to server" << endl;
                pthread_mutex_unlock(&mutex);
                close(server_fd);
                close(client_fd);
                return;
            }

            vector<char> response_data(4096);
            ssize_t bytes_received = read(server_fd, response_data.data(), response_data.size());
            if (bytes_received <= 0) {
                pthread_mutex_lock(&mutex);
                cerr << user_id << ": ERROR - Failed to receive response from server for validation" << endl;
                proxyLog << user_id << ": ERROR - Failed to receive response from server for validation" << endl;
                pthread_mutex_unlock(&mutex);
                close(server_fd);
                close(client_fd);
                return;
            }

            string serverResponseStr(response_data.begin(), response_data.begin() + bytes_received);
            // Get status line
            size_t end_pos = serverResponseStr.find("\r\n");
            string status_line;
            if (end_pos != string::npos) {
                status_line = serverResponseStr.substr(0, end_pos);
            }

            // Get status code
            string status_code;
            if (!status_line.empty()) {
                size_t status_code_start = status_line.find(" ") + 1;
                size_t status_code_end = status_line.find(" ", status_code_start);
                if (status_code_start != string::npos && status_code_end != string::npos) {
                    status_code = status_line.substr(status_code_start, status_code_end - status_code_start);
                }
            }

            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": Received \"" << status_line << "\" from " << request.host << endl;
            cout << user_id << ": Received \"" << status_line << "\" from " << request.host << endl;
            pthread_mutex_unlock(&mutex);

            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": Responding \"" << status_line << "\"" << endl;
            cout << user_id << ": Responding \"" << status_line << "\"" << endl;
            pthread_mutex_unlock(&mutex);

            if (status_code == "304") {
                if (write(client_fd, cacheResponse.response_info.data(), cacheResponse.response_info.size()) == -1) {
                    pthread_mutex_lock(&mutex);
                    cerr << user_id << ": ERROR - Failed to send cached response to client" << endl;
                    proxyLog << user_id << ": ERROR - Failed to send cached response to client" << endl;
                    pthread_mutex_unlock(&mutex);
                }
            } else {
                // We get the response from server
                // We dont update cache
                
                pthread_mutex_lock(&mutex);
                proxyLog << user_id << ": Note - Failed to validate, request from the original website again" << endl;
                cout << user_id << ": Note - Failed to validate, request from the original website again." << endl;
                pthread_mutex_unlock(&mutex);

                pthread_mutex_lock(&mutex);
                proxyLog << user_id << ": Requesting \"" << request.get_first_line() << "\" from " << request.host << endl;
                cout << user_id << ": Requesting \"" << request.get_first_line() << "\" from " << request.host << endl;
                pthread_mutex_unlock(&mutex);
                
                int server_fd = create_client_socket(request.host.c_str(), "80");
                
                if (server_fd == -1) {
                    pthread_mutex_lock(&mutex);
                    proxyLog << user_id << ": ERROR - Failed to connect to server" << endl;
                    cerr << user_id << ": ERROR - Failed to connect to server" << endl;
                    pthread_mutex_unlock(&mutex);
                    
                    close(client_fd);
                    return;
                }

                if (send(server_fd, request.request_info.data(), request.request_info.size(), 0) < 0) {
                    pthread_mutex_lock(&mutex);
                    proxyLog << user_id << ": ERROR - Failed to send request to server" << endl;
                    cerr << user_id << ": ERROR - Failed to send request to server" << endl;
                    pthread_mutex_unlock(&mutex);
                    
                    close(server_fd);
                    close(client_fd);
                    return;
                }

                vector<char> response_data(4096);
                ssize_t bytes_received = read(server_fd, response_data.data(), response_data.size());

                string responseStr(response_data.begin(), response_data.begin() + bytes_received);
                size_t end_pos = responseStr.find("\r\n");
                string status_line = (end_pos != string::npos) ? responseStr.substr(0, end_pos) : "Invalid Response Format";

                pthread_mutex_lock(&mutex);
                proxyLog << user_id << ": Received \"" << status_line << "\" from " << request.host << endl;
                cout << user_id << ": Received \"" << status_line << "\" from " << request.host << endl;
                pthread_mutex_unlock(&mutex);
                
                bool isChunked = false;
                if (findChunk(response_data.data(), bytes_received)) {
                    isChunked = true;
                    }

                if (isChunked) {
                    pthread_mutex_lock(&mutex);
                    proxyLog << user_id << ": Note - Chunked"  << endl;
                    cout << user_id << ": Note - Chunked" << endl;
                    pthread_mutex_unlock(&mutex);
                    
                    vector<char> full_chunk;
                    write(client_fd, response_data.data(), bytes_received);

                    if (bytes_received == -1) {
                        pthread_mutex_lock(&mutex);
                        proxyLog << user_id << ": ERROR - Failed to read response from server" << endl;
                        cerr << user_id << ": ERROR - Failed to read response from server" << endl;
                        pthread_mutex_unlock(&mutex);
                        
                        close(server_fd);
                        close(client_fd);
                        return;
                    }

                    pthread_mutex_lock(&mutex);
                    proxyLog << user_id << ": Responding \"" << status_line << "\"" << endl;
                    cout << user_id << ": Responding \"" << status_line << "\"" << endl;
                    pthread_mutex_unlock(&mutex);

                    full_chunk.insert(full_chunk.end(), response_data.begin(), response_data.begin() + bytes_received);
                    bool end_of_chunks = false;
                    while (!end_of_chunks) {
                        bytes_received = read(server_fd, response_data.data(), response_data.size());
                        if (bytes_received == -1) {
                            pthread_mutex_lock(&mutex);
                            proxyLog << user_id << ": ERROR - Failed to read response from server" << endl;
                            cerr << user_id << ": ERROR - Failed to read response from server" << endl;
                            pthread_mutex_unlock(&mutex);
                            
                            close(server_fd);
                            close(client_fd);
                            return;
                        }

                        if (bytes_received > 0) {
                            full_chunk.insert(full_chunk.end(), response_data.begin(), response_data.begin() + bytes_received);
                            write(client_fd, response_data.data(), bytes_received);

                            string chunk_data(response_data.begin(), response_data.begin() + bytes_received);
                            if (chunk_data.find("0\r\n\r\n") != string::npos) {
                                end_of_chunks = true;
                            }
                        } 
                    }

                    string response_str(full_chunk.begin(), full_chunk.end());
                    Response response(response_str, user_id);
                    string cache_result = response.cache_response(request.url, user_id);

                    pthread_mutex_lock(&mutex);
                    proxyLog << user_id << cache_result << endl;
                    cout << user_id << cache_result << endl;
                    pthread_mutex_unlock(&mutex);

                } else {
                    string responseStr(response_data.begin(), response_data.begin() + bytes_received);
                    Response response(responseStr, user_id);
                    
                    if (bytes_received <= 0) {
                        pthread_mutex_lock(&mutex);
                        proxyLog << user_id << ": ERROR - Failed to read response from server" << endl;
                        cerr << user_id << "Failed to read response from server" << endl;
                        pthread_mutex_unlock(&mutex);
                        
                        close(server_fd);
                        close(client_fd);
                        return;
                    }
                    
                    string cache_result = response.cache_response(request.url, user_id);
                    pthread_mutex_lock(&mutex);
                    proxyLog << user_id << cache_result << endl;
                    cout << user_id << cache_result << endl;
                    pthread_mutex_unlock(&mutex);
                    
                    pthread_mutex_lock(&mutex);
                    proxyLog << user_id << ": Responding \"" << response.status_line << "\"" << endl;
                    cout << user_id << ": Responding \"" << response.status_line << "\"" << endl;
                    pthread_mutex_unlock(&mutex);

                    size_t content_length = 0;
                    auto content_length_pos = responseStr.find("Content-Length: ");
                    if (content_length_pos != string::npos) {
                        size_t content_length_end = responseStr.find("\r\n", content_length_pos);
                        if (content_length_end != string::npos) {
                            string content_length_str = responseStr.substr(content_length_pos + 16, content_length_end - content_length_pos - 16);
                            content_length = stoul(content_length_str);
                        }
                    }

                    write(client_fd, response_data.data(), bytes_received);

                    if (content_length > 0) {
                        size_t total_bytes_read = bytes_received;
                        while (total_bytes_read < content_length) {
                            bytes_received = read(server_fd, response_data.data(), response_data.size());
                            if (bytes_received > 0) {
                                write(client_fd, response_data.data(), bytes_received);
                                total_bytes_read += bytes_received;
                            } else {
                                break;
                            }
                        }
                    } else {
                        while ((bytes_received = read(server_fd, response_data.data(), response_data.size())) > 0) {
                            write(client_fd, response_data.data(), bytes_received);
                        }
                    }
                    close(server_fd);
                    close(client_fd);
                }
            }

        //If validation is not needed
        } else {
            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": Responding \"" << cacheResponse.status_line << "\"" << endl;
            cout << user_id << ": Responding \"" << cacheResponse.status_line << "\"" << endl;
            pthread_mutex_unlock(&mutex);
            if (write(client_fd, cacheResponse.response_info.data(), cacheResponse.response_info.size()) == -1) {
                pthread_mutex_lock(&mutex);
                proxyLog << user_id << ": ERROR - Failed to send cached response to client" << endl;
                cerr << user_id << ": ERROR - Failed to send cached response to client" << endl;
                pthread_mutex_unlock(&mutex);
            }
        }
    
    //If not in cache
    } else {//If not in cache
        pthread_mutex_lock(&mutex);
        proxyLog << user_id << ": Requesting \"" << request.get_first_line() << "\" from " << request.host << endl;
        cout << user_id << ": Requesting \"" << request.get_first_line() << "\" from " << request.host << endl;
        pthread_mutex_unlock(&mutex);
        
        int server_fd = create_client_socket(request.host.c_str(), "80");
        
        if (server_fd == -1) {
            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": ERROR - Failed to connect to server" << endl;
            cerr << user_id << ": ERROR - Failed to connect to server" << endl;
            pthread_mutex_unlock(&mutex);
            
            close(client_fd);
            return;
        }

        if (send(server_fd, request.request_info.data(), request.request_info.size(), 0) < 0) {
            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": ERROR - Failed to send request to server" << endl;
            cerr << user_id << ": ERROR - Failed to send request to server" << endl;
            pthread_mutex_unlock(&mutex);
            
            close(server_fd);
            close(client_fd);
            return;
        }

        vector<char> response_data(4096);
        ssize_t bytes_received = read(server_fd, response_data.data(), response_data.size());

        string responseStr(response_data.begin(), response_data.begin() + bytes_received);
        size_t end_pos = responseStr.find("\r\n");
        string status_line = (end_pos != string::npos) ? responseStr.substr(0, end_pos) : "Invalid Response Format";

        pthread_mutex_lock(&mutex);
        proxyLog << user_id << ": Received \"" << status_line << "\" from " << request.host << endl;
        cout << user_id << ": Received \"" << status_line << "\" from " << request.host << endl;
        pthread_mutex_unlock(&mutex);

        bool isChunked = false;
        if (findChunk(response_data.data(), bytes_received)) {
            isChunked = true;
            }
        //If the response is chunked
        if (isChunked) {
            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": Note - Chunked"  << endl;
            cout << user_id << ": Note - Chunked" << endl;
            pthread_mutex_unlock(&mutex);

            vector<char> full_chunk;
            write(client_fd, response_data.data(), bytes_received);

            if (bytes_received == -1) {
                pthread_mutex_lock(&mutex);
                proxyLog << user_id << ": ERROR - Failed to read response from server" << endl;
                cerr << user_id << ": ERROR - Failed to read response from server" << endl;
                pthread_mutex_unlock(&mutex);
                
                close(server_fd);
                close(client_fd);
                return;
            }

            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": Responding \"" << status_line << "\"" << endl;
            cout << user_id << ": Responding \"" << status_line << "\"" << endl;
            pthread_mutex_unlock(&mutex);

            full_chunk.insert(full_chunk.end(), response_data.begin(), response_data.begin() + bytes_received);
            bool end_of_chunks = false;
            while (!end_of_chunks) {
                bytes_received = read(server_fd, response_data.data(), response_data.size());
                if (bytes_received == -1) {
                        pthread_mutex_lock(&mutex);
                        proxyLog << user_id << ": ERROR - Failed to read response from server" << endl;
                        cerr << user_id << ": ERROR - Failed to read response from server" << endl;
                        pthread_mutex_unlock(&mutex);
                        
                        close(server_fd);
                        close(client_fd);
                        return;
                    }

                if (bytes_received > 0) {
                    full_chunk.insert(full_chunk.end(), response_data.begin(), response_data.begin() + bytes_received);
                    write(client_fd, response_data.data(), bytes_received);

                    string chunk_data(response_data.begin(), response_data.begin() + bytes_received);
                    if (chunk_data.find("0\r\n\r\n") != string::npos) {
                        end_of_chunks = true;
                    }
                } 
            }

            string response_str(full_chunk.begin(), full_chunk.end());
            Response response(response_str, user_id);
            string cache_result = response.cache_response(request.url, user_id);

            pthread_mutex_lock(&mutex);
            proxyLog << user_id << cache_result << endl;
            cout << user_id << cache_result << endl;
            pthread_mutex_unlock(&mutex);
            
        //If the response is not chunked
        } else {//If the response is not chunked
            string responseStr(response_data.begin(), response_data.begin() + bytes_received);
            Response response(responseStr, user_id);
            
            if (bytes_received <= 0) {
                pthread_mutex_lock(&mutex);
                proxyLog << user_id << ": ERROR - Failed to read response from server" << endl;
                cerr << user_id << "Failed to read response from server" << endl;
                pthread_mutex_unlock(&mutex);
                
                close(server_fd);
                close(client_fd);
                return;
            }
            
            string cache_result = response.cache_response(request.url, user_id);
            pthread_mutex_lock(&mutex);
            proxyLog << user_id << cache_result << endl;
            cout << user_id << cache_result << endl;
            pthread_mutex_unlock(&mutex);
            

            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": Responding \"" << response.status_line << "\"" << endl;
            cout << user_id << ": Responding \"" << response.status_line << "\"" << endl;
            pthread_mutex_unlock(&mutex);

            size_t content_length = 0;
            auto content_length_pos = responseStr.find("Content-Length: ");
            if (content_length_pos != string::npos) {
                size_t content_length_end = responseStr.find("\r\n", content_length_pos);
                if (content_length_end != string::npos) {
                    string content_length_str = responseStr.substr(content_length_pos + 16, content_length_end - content_length_pos - 16);
                    content_length = stoul(content_length_str);
                }
            }

            write(client_fd, response_data.data(), bytes_received);

            if (content_length > 0) {
                size_t total_bytes_read = bytes_received;
                while (total_bytes_read < content_length) {
                    bytes_received = read(server_fd, response_data.data(), response_data.size());
                    if (bytes_received > 0) {
                        write(client_fd, response_data.data(), bytes_received);
                        total_bytes_read += bytes_received;
                    } else {
                        break;
                    }
                }
            } else {
                while ((bytes_received = read(server_fd, response_data.data(), response_data.size())) > 0) {
                    write(client_fd, response_data.data(), bytes_received);
                }
            }
            close(server_fd);
            close(client_fd);
        }
    }
}





void Proxy::handle_POST(int client_fd, Request& request, int user_id) {
    pthread_mutex_lock(&mutex);
    proxyLog << user_id << ": Requesting \"" << request.get_first_line() << "\" from " << request.host << endl;
    cout << user_id << ": Requesting \"" << request.get_first_line() << "\" from " << request.host << endl;
    pthread_mutex_unlock(&mutex);
    
    int server_fd = create_client_socket(request.host.c_str(), "80");
    if (server_fd == -1) {
        cerr << "Failed to connect to server" << endl;
        close(client_fd);
        return;
    }

    string request_str(request.request_info.begin(), request.request_info.end());
    if (write(server_fd, request_str.c_str(), request_str.size()) == -1) {
        pthread_mutex_lock(&mutex);
        cerr << "Failed to send request to server" << endl;
        proxyLog << user_id << ": ERROR - Failed to send request to server" << endl;
        pthread_mutex_unlock(&mutex);
        close(server_fd);
        close(client_fd);
        return;
    }

    vector<char> response_data(4096);
    int bytes_received; 
    bytes_received = read(server_fd, response_data.data(), response_data.size());
    // cout.write(response_data.data(), bytes_received);
    Response response(string(response_data.begin(), response_data.begin() + bytes_received), user_id);
    pthread_mutex_lock(&mutex);
    proxyLog << user_id << ": Received \"" << response.status_line << "\" from " << request.host << endl;
    cout << user_id << ": Received \"" << response.status_line << "\" from " << request.host << endl;
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    proxyLog << user_id << ": Responding \"" << response.status_line << "\"" << endl;
    cout << user_id << ": Responding \"" << response.status_line << "\"" << endl;
    pthread_mutex_unlock(&mutex);

    if (bytes_received == -1) {
        pthread_mutex_lock(&mutex);
        cerr << "Failed to read response from server" << endl;
        proxyLog << user_id << ": ERROR - Failed to read response from server" << endl;
        pthread_mutex_unlock(&mutex);
    }

    if (write(client_fd, response_data.data(), bytes_received) == -1) {
        pthread_mutex_lock(&mutex);
        cerr << "Failed to send response to client" << endl;
        proxyLog << user_id << ": ERROR - Failed to send response to client" << endl;
        pthread_mutex_unlock(&mutex);
    }


    close(server_fd);
    close(client_fd);
}


void Proxy::handle_CONNECT(int client_fd, Request& request, int user_id) {
    pthread_mutex_lock(&mutex);
    proxyLog << user_id << ": Requesting \"" << request.get_first_line() << "\" from " << request.host << endl;
    cout << user_id << ": Requesting \"" << request.get_first_line() << "\" from " << request.host << endl;
    pthread_mutex_unlock(&mutex);
    
    int server_fd = create_client_socket(request.host.c_str(), "443");
    if (server_fd == -1) {
        pthread_mutex_lock(&mutex);
        proxyLog << user_id << ": ERROR - Failed to connect to server" << endl;
        cerr << user_id << ": ERROR - Failed to connect to server" << endl;
        pthread_mutex_unlock(&mutex);
        
        close(client_fd);
        return;
    }
    pthread_mutex_lock(&mutex);
    proxyLog << user_id << ": Responding \"HTTP/1.1 200 OK\"" << endl;
    cout << user_id << ": Responding \"HTTP/1.1 200 OK\"" << endl;
    pthread_mutex_unlock(&mutex);

    const char* success_response = "HTTP/1.1 200 Connection Established\r\n\r\n";
    if (write(client_fd, success_response, strlen(success_response)) == -1) {
        pthread_mutex_lock(&mutex);
        proxyLog << user_id << ": ERROR - Failed to send response to client" << endl;
        cerr << "Failed to send response to client" << endl;
        pthread_mutex_unlock(&mutex);
        
        close(server_fd);
        close(client_fd);
        return;
    }

    fd_set read_fds;
    int max_fd = max(client_fd, server_fd) + 1;
    char buffer[4096];

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(client_fd, &read_fds);
        FD_SET(server_fd, &read_fds);

        if (select(max_fd, &read_fds, NULL, NULL, NULL) == -1) {
            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": ERROR - Select error" << endl;
            cerr << user_id << ": ERROR - Select error" << endl;
            pthread_mutex_unlock(&mutex);
            
            break;
        }

        if (FD_ISSET(client_fd, &read_fds)) {
            int bytes_received = read(client_fd, buffer, sizeof(buffer));
            if (bytes_received <= 0) {
                break;
            }
            if (write(server_fd, buffer, bytes_received) == -1) {
                pthread_mutex_lock(&mutex);
                proxyLog << user_id << ": ERROR - Failed to write to server" << endl;
                cerr << user_id << ": ERROR - Failed to write to server" << endl;
                pthread_mutex_unlock(&mutex);
                
                break;
            }
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            int bytes_received = read(server_fd, buffer, sizeof(buffer));
            if (bytes_received <= 0) {
                break;
            }
            if (write(client_fd, buffer, bytes_received) == -1) {
                pthread_mutex_lock(&mutex);
                proxyLog << user_id << ": ERROR - Failed to write to client" << endl;
                cerr << user_id << ": ERROR -Failed to write to client" << endl;
                pthread_mutex_unlock(&mutex);
                
                break;
            }
        }
    
    }
    
    close(server_fd);
    close(client_fd);
    
}


// Validation request
string Proxy::constructConditionalRequest(const Request& originalRequest, const Response& cachedResponse) {
    ostringstream requestStream;
    requestStream << originalRequest.request_method << " " << originalRequest.url << " " << originalRequest.http_type << "\r\n";
    requestStream << "Host: " << originalRequest.host << "\r\n";

    if (!cachedResponse.etag.empty()) {
        requestStream << "If-None-Match: " << cachedResponse.etag << "\r\n";
    }
    if (!cachedResponse.last_modified.empty()) {
        requestStream << "If-Modified-Since: " << cachedResponse.last_modified << "\r\n";
    }
    requestStream << "\r\n";

    return requestStream.str();
}

bool Proxy::findChunk(char *server_msg, int mes_len) {
    string msg(server_msg, mes_len);
    size_t separator = msg.find("\r\n\r\n");
    if (separator != string::npos) {
        string headers = msg.substr(0, separator);
        size_t pos = headers.find("Transfer-Encoding: chunked");
        if (pos != string::npos) {
            if (pos == 0 || headers[pos - 1] == '\n') {
                return true;
            }
        }
    }

    return false;
}


string Proxy::getTime() {
    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime);
    timeinfo = gmtime(&rawtime);

    string str(asctime(timeinfo));
    if (!str.empty() && str.back() == '\n') {
        str.pop_back(); 
    }

    return str;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }

    Proxy proxy(argv[1]);
    proxy.start();

    return 0;
}
