#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <random>
#include <algorithm>
#include <cstring>
#include <iomanip> 
#include <fcntl.h>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

class TCPClient {
public:
    TCPClient(const std::string& addr, int port, int n, int connections);
    ~TCPClient();

    bool run();

private:
    void generateExpressions(std::vector<std::string>& expressions, std::vector<double>& expected_results);
    void sendExpression(int sock, const std::string& expr);
    std::vector<std::string> splitExpression(const std::string& expr);
    void verifyResult(const std::string& expr, const std::string& response, double expected);

    std::string serverAddr;
    int serverPort;
    int n;
    int connections;
    int epollFd;
    std::vector<std::pair<int, size_t>> socketToExpr; 
};

#endif 
