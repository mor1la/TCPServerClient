#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <queue>
#include <stack>
#include <unordered_map>
#include <algorithm>
#include <iomanip> 

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

class TCPServer {
public:
    TCPServer(int port);
    ~TCPServer();

    bool start();
    void run();

private:
    bool setNonblocking(int fd);
    void handleNewConnection();
    void handleClient(int clientFd);
    void processExpressions(int clientFd);
    void sendResponse(int clientFd, const std::string& response);
    void closeConnection(int client_fd);
    std::string calculateExpression(const std::string& expr);

    int port;
    int epollFd;
    int serverFd;
    std::unordered_map<int, std::string> clientBuffers;
};

#endif 
