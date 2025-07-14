#include "TcpServer.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    TCPServer server(port);

    if (!server.start()) {
        return 1;
    }

    std::cout << "Server started on port " << port << std::endl;
    server.run();

    return 0;
}
