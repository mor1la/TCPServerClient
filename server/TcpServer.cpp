#include "TcpServer.h"

TCPServer::TCPServer(int port) : port(port), epollFd(-1), serverFd(-1) {}

TCPServer::~TCPServer() {
    if (epollFd != -1) close(epollFd);
    if (serverFd != -1) close(serverFd);
}

bool TCPServer::start() {
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) {
        perror("socket");
        return false;
    }

    int opt = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        return false;
    }

    if (!setNonblocking(serverFd)) {
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(serverFd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind");
        return false;
    }

    if (listen(serverFd, 10) < 0) {
        perror("listen");
        return false;
    }

    epollFd = epoll_create1(0);
    if (epollFd == -1) {
        perror("epoll_create1");
        return false;
    }

    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = serverFd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &event) == -1) {
        perror("epoll_ctl: serverFd");
        return false;
    }

    return true;
}

void TCPServer::run() {
    epoll_event events[MAX_EVENTS];

    while (true) {
        int numEvents = epoll_wait(epollFd, events, MAX_EVENTS, -1);
        if (numEvents == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < numEvents; ++i) {
            if (events[i].data.fd == serverFd) {
                handleNewConnection();
            } else {
                handleClient(events[i].data.fd);
            }
        }
    }
}

bool TCPServer::setNonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return false;
    }

    return true;
}

void TCPServer::handleNewConnection() {
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientFd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                perror("accept");
                break;
            }
        }

        if (!setNonblocking(clientFd)) {
            close(clientFd);
            continue;
        }

        epoll_event event{};
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        event.data.fd = clientFd;
        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1) {
            perror("epoll_ctl: clientFd");
            close(clientFd);
            continue;
        }

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
        std::cout << "New connection from " << ipStr << ":" << ntohs(clientAddr.sin_port) << std::endl;
    }
}

void TCPServer::handleClient(int clientFd) {
    std::string buffer(BUFFER_SIZE, '\0'); 
    ssize_t bytesRead;

    while ((bytesRead = recv(clientFd, &buffer[0], BUFFER_SIZE - 1, 0)) > 0) {
        if (bytesRead == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; 
            } else {
                perror("recv");
                closeConnection(clientFd);
                return;
            }
        }

        buffer.resize(bytesRead); 
        clientBuffers[clientFd] += buffer;

        processExpressions(clientFd);
    }

    if (bytesRead == 0) {
        closeConnection(clientFd); 
    }
}

void TCPServer::processExpressions(int clientFd) {
    size_t pos;
    std::string& buffer = clientBuffers[clientFd];

    while ((pos = buffer.find(' ')) != std::string::npos) {
        std::string expression = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);

        if (!expression.empty()) {
            std::cout << "Client " << clientFd << " sent expression: " << expression << std::endl;

            std::string result = calculateExpression(expression);

            std::cout << "Result for client " << clientFd << ": " << result;

            sendResponse(clientFd, result);
        }
    }
}

void TCPServer::sendResponse(int clientFd, const std::string& response) {
    std::string fullResponse = response + " ";
    ssize_t bytesSent = send(clientFd, fullResponse.c_str(), fullResponse.size(), 0);
    if (bytesSent == -1) {
        perror("send");
        closeConnection(clientFd);
    }
}

void TCPServer::closeConnection(int clientFd) {
    epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);
    close(clientFd);
    clientBuffers.erase(clientFd);
    std::cout << "Connection closed" << std::endl;
}

std::string TCPServer::calculateExpression(const std::string& expr) {
    try {
        std::vector<double> numbers;
        std::vector<char> operations;

        std::istringstream iss(expr);
        double num;
        char op;
        
        iss >> num;
        numbers.push_back(num);
        
        while (iss >> op >> num) {
            operations.push_back(op);
            numbers.push_back(num);
        }

        std::vector<double> evalStack;
        std::vector<char> opStack;
        
        evalStack.push_back(numbers[0]);
        
        for (size_t j = 0; j < operations.size(); ++j) {
            char currentOp = operations[j];
            double currentNum = numbers[j+1];
            
            if (currentOp == '*' || currentOp == '/') {
                double& top = evalStack.back();
                if (currentOp == '*') {
                    top *= currentNum;
                } else {
                    if (currentNum == 0) return "ERROR: Division by zero\n";
                    top /= currentNum;
                }
            } 
            else {
                evalStack.push_back(currentNum);
                opStack.push_back(currentOp);
            }
        }
        
        double result = evalStack[0];
        for (size_t j = 0; j < opStack.size(); ++j) {
            if (opStack[j] == '+') {
                result += evalStack[j+1];
            } else {
                result -= evalStack[j+1];
            }
        }

        return std::to_string(result) + "\n";
    } catch (...) {
        return "ERROR: Invalid expression\n";
    }
}