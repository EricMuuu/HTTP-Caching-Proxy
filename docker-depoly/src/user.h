#include <pthread.h>

#include <string>

#include <netinet/in.h> 

class User{
private:
    #include <string>

    int user_id;
    int user_fd;
    std::string user_ip;

    public:
    int get_id() { return user_id; }
    int get_fd() { return user_fd; }
    std::string get_ip() { return user_ip; }
    
    void set_id(int id) { user_id = id; }
    void set_fd(int fd) { user_fd = fd; }
    void set_ip(std::string ip) { user_ip = ip; }

};