# Компилятор и флаги
CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -I.

# Имена исполняемых файлов
CLIENT_EXE := client/client
SERVER_EXE := server/server

# Исходные файлы клиента
CLIENT_SRCS := client/main.cpp client/TcpClient.cpp
CLIENT_OBJS := $(CLIENT_SRCS:.cpp=.o)

# Исходные файлы сервера
SERVER_SRCS := server/main.cpp server/TcpServer.cpp
SERVER_OBJS := $(SERVER_SRCS:.cpp=.o)

# Правило по умолчанию
all: $(CLIENT_EXE) $(SERVER_EXE)

# Сборка клиента
$(CLIENT_EXE): $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Сборка сервера
$(SERVER_EXE): $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Правило для объектных файлов клиента
client/%.o: client/%.cpp client/TcpClient.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Правило для объектных файлов сервера
server/%.o: server/%.cpp server/TcpServer.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Очистка
clean:
	rm -f $(CLIENT_OBJS) $(SERVER_OBJS) $(CLIENT_EXE) $(SERVER_EXE)

.PHONY: all clean
