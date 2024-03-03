#include "request.h"

using namespace std;

// Extract the full info of the request
void Request::extract_request(string request){
    this->request_info.assign(request.begin(), request.end());
}

// TODO: handel error, is cerr good enough?
// Extract the method and assign port
void Request::extract_method(){
    string request(this->request_info.begin(), this->request_info.end());
    size_t end_pos = request.find("\r\n");
    // If it is not empty
    if (end_pos != string::npos) {
        string first_line = request.substr(0, end_pos);
        size_t s = first_line.find(" ");
        if (s != string::npos){
            this->request_method = first_line.substr(0, s);
        }
        if (this->request_method != "GET" && this->request_method != "POST" && this->request_method != "CONNECT"){
            this->is_valid = 0;
            cerr << "Method must be one of GET, POST or CONNECT" << endl;
        } else if (this->request_method == "CONNECT") {
            this->server_port = 443;
        } else {
            this->server_port = 80;
        }
    } else {
        cerr << "Format of this request if invalid" << endl;
        this->is_valid = 0;
    }
}


string Request::get_first_line() {
    string request(this->request_info.begin(), this->request_info.end());
    size_t end_pos = request.find("\r\n");
    string first_line = request.substr(0, end_pos);
    return first_line;
}

// Track the HTTP type
void Request::extract_HTTP(){
    string request = this->request_info.data();
    size_t end_pos = request.find("\r\n");
    // If it is not empty
    if (end_pos != string::npos) {
        string first_line = request.substr(0, end_pos);
        // Find the second space, where http type begins
        size_t s = first_line.find_first_of(" ", first_line.find_first_of(" ") + 1);
        if (s != string::npos) {
            this->http_type = first_line.substr(s + 1);
            // TODO: should we check for HTTP version?
        } else {
            cerr << "Invalid HTTP type found" << endl;
            this->is_valid = 0;
        }
    }
}

// Track the host
void Request::extract_host(){
    string request = this->request_info.data();
    size_t start_pos = request.find("Host:");
    if (start_pos != string::npos) {
        // Move to the actual host name
        start_pos += 6;
        size_t end_pos = request.find("\r\n", start_pos);
        if (end_pos != string::npos) {
            string cur_host = request.substr(start_pos, end_pos - start_pos);
            // If port specified
            size_t port_pos = cur_host.find(":");
            if (port_pos != string::npos) {
                cur_host = cur_host.substr(0, port_pos);
            }
            this->host = cur_host;
        } else {
            cerr << "Invalid Host found" << endl;
            this->is_valid = 0;
        }
    } else {
        cerr << "Invalid Host found" << endl;
        this->is_valid = 0;
    }
}

void Request::extract_target() {
    size_t method_end = this->request_method.size() + 1;
    string requestString(this->request_info.begin(), this->request_info.end());
    size_t http_start = requestString.find(" HTTP");
    if (method_end != string::npos && http_start != string::npos && http_start > method_end) {
        this->request_target = string(this->request_info.begin() + method_end, this->request_info.begin() + http_start);
    } else {
        cerr << "Invalid request format" << endl;
        this->is_valid = 0;
    }
}


void Request::extract_url() {
    if (this->request_method == "CONNECT") {
        this->url = this->host;
    } else {
        string protocol = (this->server_port == 443) ? "https://" : "http://";
        url = protocol + this->host;
        if (!this->request_target.empty() && this->request_target != "/") {
            this->url += this->request_target;
        }
    }
}