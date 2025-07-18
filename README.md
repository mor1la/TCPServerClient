# Реализация TCP-сервера и клиента с использованием epoll

## Реализованные компоненты

1. **TCP сервер — калькулятор**  
   Принимает на вход числовые арифметические выражения и возвращает результат вычисления.  
   - Выражения **разделяются пробелами**.  
   - Внутри выражения **нет пробелов**.  
   - Сервер конфигурируется **только портом прослушивающего сокета**.

2. **TCP клиент — верификатор**  
   Выполняет функцию проверки корректности работы сервера.  
   - Генерирует строку арифметического выражения из `n` чисел.  
   - Делит её на **случайное количество фрагментов**.  
   - Последовательно отправляет эти фрагменты в сокет.  
   - Получает ответ от сервера и сравнивает его с корректным результатом.  
   - В случае ошибки выводит в `std::cerr`:
     - исходное выражение,  
     - ответ сервера,  
     - правильный ответ.  

   Клиент конфигурируется следующими параметрами (через аргументы командной строки):
   - `n` — количество чисел в выражении;
   - `connections` — количество TCP-сессий к серверу;
   - `server_addr` — IP-адрес TCP-сервера;
   - `server_port` — порт TCP-сервера.

## Используемые технологии

- **epoll** используется **в обоих приложениях** для масштабируемой работы с сокетами.

---

## Архитектура проекта

```bash
.
├── client
│   ├── main.cpp
│   ├── TcpClient.cpp
│   └── TcpClient.h
├── Makefile
└── server
    ├── main.cpp
    ├── TcpServer.cpp
    └── TcpServer.h
```
# Сборка проекта

Для сборки проекта необходимо выполнить команду:

```bash
make
```
# Пример запуска
В одном терминале запустите сервер:
```bash
./server/server 8080
```
В другом терминале запустите клиент:
```bash
./client/client 5 10 127.0.0.1 8080
```

