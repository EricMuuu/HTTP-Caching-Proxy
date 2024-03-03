#include "response.h"
#include "cache.h"
#include "common.h"

using namespace std;

// Extract the full info of the response
// TODO: check response content????
void Response::extract_response(string response){
    this->response_info.assign(response.begin(), response.end());
}

void Response::extract_status_line(){
    string cur_response(this->response_info.begin(), this->response_info.end());
    size_t end_pos = cur_response.find("\r\n");
    if (end_pos != string::npos) {
        this->status_line = string(this->response_info.begin(), this->response_info.begin() + end_pos);
    } else {
        pthread_mutex_lock(&mutex);
        proxyLog << user_id << ": ERROR - Cannot find the status line" << endl;
        cerr << "Cannot find the status line" << endl;
        pthread_mutex_unlock(&mutex);
        this->is_valid = 0; 
    }
}

// TODO: should have values ready when init

void Response::extract_status_code(){
    string cur_response(this->response_info.begin(), this->response_info.end());
    size_t end_pos = cur_response.find("\r\n");
    if (end_pos != string::npos) {
        string first_line = string(this->response_info.begin(), this->response_info.begin() + end_pos);
        size_t space = first_line.find(" ");
        if (space != string::npos) {
            size_t code_start = space + 1;
            size_t code_end = first_line.find(" ", code_start);
            if (code_end != string::npos) {
                this->status_code = first_line.substr(code_start, code_end - code_start);
            } else {
                pthread_mutex_lock(&mutex);
                proxyLog << user_id << ": ERROR - Cannot extract status code" << endl;
                cerr << "Cannot extract status code" << endl;
                pthread_mutex_unlock(&mutex);
                this->is_valid = 0;
            }
        } else {
            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": ERROR - Cannot find status code" << endl;
            cerr << "Cannot find status code" << endl;
            pthread_mutex_unlock(&mutex);
            this->is_valid = 0;
        }
    } else {
        pthread_mutex_lock(&mutex);
        proxyLog << user_id << ": ERRPR - Cannot find the header line" << endl;
        cerr << "Cannot find the header line" << endl;
        pthread_mutex_unlock(&mutex);
        this->is_valid = 0;
    }
}

// TODO: CHECK
void Response::extract_cache_control(){
    string cur_response(this->response_info.begin(), this->response_info.end());
    size_t control_pos = cur_response.find("Cache-Control: ");
    if (control_pos != string::npos) {
        size_t end_pos = cur_response.find("\r\n", control_pos);
        if (end_pos != string::npos) {
            this->cache_control = cur_response.substr(control_pos + 15, end_pos - control_pos - 15);
        } else {
            pthread_mutex_lock(&mutex);
            proxyLog << user_id << ": ERROR - Cannot extract cache control policy" << endl;
            cerr << "Cannot extract cache control policy" << endl;
            pthread_mutex_unlock(&mutex);
            this->is_valid = 0;
        }
    } 
}


void Response::extract_date(){
    string cur_response(this->response_info.begin(), this->response_info.end());
    size_t date_pos = cur_response.find("Date: ");
    if (date_pos != string::npos) {
        size_t end_pos = cur_response.find("\r\n", date_pos);
        if (end_pos != string::npos) {
            this->date = cur_response.substr(date_pos + 6, end_pos - date_pos - 6);
        }
    }
}

void Response::extract_age() {
    if (!this->cache_control.empty()) {
        size_t max_age_pos = this->cache_control.find("max-age=");
        if (max_age_pos != string::npos) {
            size_t start = max_age_pos + 8;
            size_t end = this->cache_control.find(",", start);
            if (end == string::npos) {
                end = this->cache_control.length();
            }
            this->age = this->cache_control.substr(start, end - start);
        } else {
            this->age = "";
        }
    }
}


void Response::extract_expire(){
    string cur_response(this->response_info.begin(), this->response_info.end());
    size_t expire_pos = cur_response.find("Expires: ");
    if (expire_pos != string::npos) {
        size_t end_pos = cur_response.find("\r\n", expire_pos);
        if (end_pos != string::npos) {
            this->expire = cur_response.substr(expire_pos + 9, end_pos - expire_pos - 9);
        }
    }
}

void Response::extract_etag(){
    string cur_response(this->response_info.begin(), this->response_info.end());
    size_t etag_pos = cur_response.find("ETag: ");
    if (etag_pos != string::npos) {
        size_t end_pos = cur_response.find("\r\n", etag_pos);
        if (end_pos != string::npos) {
            this->etag = cur_response.substr(etag_pos + 6, end_pos - etag_pos - 6);
        }
    }
}

void Response::extract_modify(){
    string cur_response(this->response_info.begin(), this->response_info.end());
    size_t modify_pos = cur_response.find("Last-Modified: ");
    if (modify_pos != string::npos) {
        size_t end_pos = cur_response.find("\r\n", modify_pos);
        if (end_pos != string::npos) {
            this->last_modified = cur_response.substr(modify_pos + 15, end_pos - modify_pos - 15);
        }
    }
}

// void Response::extract_content(){
//     string cur_response(this->response_info.begin(), this->response_info.end());
//     // End position of the entire header
//     size_t header_end = cur_response.find("\r\n\r\n");
//     if (header_end == string::npos) {
//         cerr << "Invalid format of response" << endl;
//         is_valid = 0;
//     }
//     size_t length_start = cur_response.find("Content-Length: ");
//     if (length_start == string::npos) {
//         cerr << "Cannot locate content length" << endl;
//         is_valid = 0;
//     }
//     size_t length_end = cur_response.find("\r\n", length_start);
//     if (length_end == string::npos) {
//         cerr << "Cannot extract content length" << endl;
//         is_valid = 0;
//     }
//     int content_length = stoi(cur_response.substr(length_start + 16, length_end - length_start - 16));
//     size_t content_start = header_end + 4;
//     //size_t content_end = content_start + content_length;
//     this->response_content.assign(cur_response.begin() + content_start, cur_response.begin() + content_start + content_length);
// }

string Response::cache_response(const string& requestUrl, int user_id) {
    // if code is 200 but the response is invalid
    // How do we use the request URL as the key here?????
    string key = requestUrl;
    if (this->is_valid != 1 && this->status_code == "200") {
        return ": not cacheable because the response is invalid";
    }
    else if (this->cache_control.find("no-store") != string::npos || 
             this->cache_control.find("private") != string::npos) {
        return ": not cacheable because the cache-control is no-store or private";
    }
    if (this->cache_control.find("no-cache") != string::npos || 
        this->cache_control.find("must-revalidate") != string::npos ||
        !this->etag.empty() || !this->last_modified.empty()) {
        add_to_cache(key, *this, user_id);
        return ": cached, but requires re-validation";
    }
    else if (!this->expire.empty() || !this->age.empty()) {
        add_to_cache(key, *this, user_id);
        auto& entry = cache.at(key);
        string expirationDetail = "expires at " + expirationTimeToString(calculateExpirationTime(entry));
        return ": cached, " + expirationDetail;
    }
    return ": undefined cache policy"; 
}