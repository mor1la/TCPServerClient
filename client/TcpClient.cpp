#include "TcpClient.h"

TCPClient::TCPClient(const std::string& addr, int port, int n, int connections) 
    : serverAddr(addr), serverPort(port), n(n), connections(connections), epollFd(-1) {}

TCPClient::~TCPClient() {
    if (epollFd != -1) close(epollFd);
}

bool TCPClient::run() {
    epollFd = epoll_create1(0);
    if (epollFd == -1) {
        perror("epoll_create1");
        return false;
    }

    std::vector<int> sockets;
    for (int i = 0; i < connections; ++i) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            perror("socket");
            continue;
        }

        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl");
            close(sock);
            continue;
        }

        sockaddr_in servAddr{};
        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(serverPort);

        if (inet_pton(AF_INET, serverAddr.c_str(), &servAddr.sin_addr) <= 0) {
            perror("inet_pton");
            close(sock);
            continue;
        }

        if (connect(sock, (sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
            if (errno != EINPROGRESS) {
                perror("connect");
                close(sock);
                continue;
            }
        }

        epoll_event event{};
        event.events = EPOLLIN | EPOLLOUT | EPOLLET;
        event.data.fd = sock;

        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, sock, &event) == -1) {
            perror("epoll_ctl: sock");
            close(sock);
            continue;
        }

        sockets.push_back(sock);
    }

    if (sockets.empty()) {
        std::cerr << "No connections established" << std::endl;
        return false;
    }

    std::vector<std::string> expressions;
    std::vector<double> expectedResults;
    generateExpressions(expressions, expectedResults);

    for (size_t i = 0; i < sockets.size() && i < expressions.size(); ++i) {
        sendExpression(sockets[i], expressions[i]);
    }

    epoll_event events[MAX_EVENTS];
    int remaining = std::min(static_cast<int>(expressions.size()), connections);

    while (remaining > 0) {
        int numEvents = epoll_wait(epollFd, events, MAX_EVENTS, -1);
        if (numEvents == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < numEvents; ++i) {
            int sock = events[i].data.fd;

            if (events[i].events & EPOLLIN) {
                std::string response;
                char buffer[BUFFER_SIZE];

                while (true) {
                    ssize_t bytesRead = recv(sock, buffer, BUFFER_SIZE - 1, 0);
                    if (bytesRead == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        } else {
                            perror("recv");
                            epoll_ctl(epollFd, EPOLL_CTL_DEL, sock, nullptr);
                            close(sock);
                            remaining--;
                            break;
                        }
                    } else if (bytesRead == 0) {
                        epoll_ctl(epollFd, EPOLL_CTL_DEL, sock, nullptr);
                        close(sock);
                        remaining--;
                        break;
                    } else {
                        buffer[bytesRead] = '\0';
                        response += buffer;
                    }
                }

                if (!response.empty()) {
                    auto it = std::find_if(socketToExpr.begin(), socketToExpr.end(),
                        [sock](const auto& pair) { return pair.first == sock; });

                    if (it != socketToExpr.end()) {
                        size_t idx = it->second;
                        verifyResult(expressions[idx], response, expectedResults[idx]);
                        remaining--;
                    }
                }
            }

            if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                epoll_ctl(epollFd, EPOLL_CTL_DEL, sock, nullptr);
                close(sock);
                remaining--;
            }
        }
    }

    for (int sock : sockets) {
        close(sock);
    }

    return true;
}


void TCPClient::generateExpressions(std::vector<std::string>& expressions, std::vector<double>& expectedResults) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> numDist(1, 100);
    std::uniform_int_distribution<> opDist(0, 3);

    const char ops[] = {'+', '-', '*', '/'};

    for (int i = 0; i < connections; ++i) {
        std::ostringstream oss;
        std::vector<double> numbers;
        std::vector<char> operations;

        double firstNum = numDist(gen);
        numbers.push_back(firstNum);
        oss << firstNum;

        for (int j = 1; j < n; ++j) {
            char op = ops[opDist(gen)];
            operations.push_back(op);
            double num = numDist(gen);
            
            if (op == '/') {
                while (num == 0) num = numDist(gen);
            }
            
            numbers.push_back(num);
            oss << op << num; 
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

        expressions.push_back(oss.str());
        expectedResults.push_back(result);
        socketToExpr.emplace_back(-1, i);
    }
}

void TCPClient::sendExpression(int sock, const std::string& expr) {
    std::vector<std::string> fragments = splitExpression(expr);

    for (const auto& fragment : fragments) {
        const char* data = fragment.c_str();
        size_t totalSent = 0;
        size_t toSend = fragment.size();

        while (totalSent < toSend) {
            ssize_t sent = send(sock, data + totalSent, toSend - totalSent, 0);
            if (sent == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                } else {
                    perror("send");
                    break;
                }
            }
            totalSent += sent;
        }

        usleep(1000);  
    }

    const char space = ' ';
    send(sock, &space, 1, 0);

    auto it = std::find_if(socketToExpr.begin(), socketToExpr.end(),
        [sock](const auto& pair) { return pair.first == -1; });
    if (it != socketToExpr.end()) {
        it->first = sock;
    }
}


std::vector<std::string> TCPClient::splitExpression(const std::string& expr) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(2, 5);

    int fragmentsCount = dist(gen);
    std::vector<std::string> fragments;
    size_t pos = 0;
    size_t step = expr.size() / fragmentsCount;

    for (int i = 0; i < fragmentsCount - 1; ++i) {
        size_t endPos = pos + step;
        fragments.push_back(expr.substr(pos, endPos - pos));
        pos = endPos;
    }
    fragments.push_back(expr.substr(pos));

    return fragments;
}

void TCPClient::verifyResult(const std::string& expr, const std::string& response, double expected) {
    try {
        double serverResult = std::stod(response);
        if (fabs(serverResult - expected) > 1e-6) {
            std::cerr << "ERROR: Expression: " << expr 
                      << " Server result: " << serverResult
                      << " Expected: " << expected << std::endl;
        } else {
            std::cout << "OK: " << expr << " = " << expected << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Invalid server response for expression " << expr 
                  << ": " << response << std::endl;
    }
}