#include "ProxyServer.h"

#include <cstring>
#include <iostream>
#include <netdb.h>
#include <thread>
#include <unistd.h> // system commands close() sockets
#include <arpa/inet.h> // network functions like socket(), bind(), listen(), accept()

ProxyServer::ProxyServer(int listenPort, const std::string& targetHost, int targetPort)
    : listenPort_(listenPort), targetHost_(targetHost), targetPort_(targetPort) {}

void ProxyServer::start() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        return;
    }

    sockaddr_in addr {}; // struct that holds ipv4 + port
    addr.sin_family = AF_INET; // ipv4
    addr.sin_addr.s_addr = INADDR_ANY; // bind to all interfaces 0.0.0.0
    addr.sin_port = htons(listenPort_); // format port number to big endiand

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return;
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        return;
    }

    std::cout << "[*] Listening on port " << listenPort_ << std::endl;

    while (true) {
        int clientSocket = accept(server_fd, nullptr, nullptr);
        if (clientSocket < 0) {
            perror("Accept failed");
            continue;
        }

        std::thread(&ProxyServer::handleClient, this, clientSocket).detach();
    }
}

void ProxyServer::handleClient(int clientSocket) {
    std::cout << "[*] New client connected. Preparing to connect to target server..." << std::endl;

    // Create socket to Target Server
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("[!] Socket to target failed");
        close(clientSocket);
        return;
    }
    std::cout << "[+] Created socket to target server." << std::endl;

    // Resolve Target Hostname
    struct hostent* target = gethostbyname(targetHost_.c_str());
    if (target == nullptr) {
        std::cerr << "[!] Failed to resolve hostname: " << targetHost_ << std::endl;
        close(clientSocket);
        close(serverSocket);
        return;
    }
    std::cout << "[+] Resolved hostname " << targetHost_ << " to IP: "
              << inet_ntoa(*(struct in_addr*)target->h_addr) << std::endl;

    // Prepare sockaddr_in for target
    sockaddr_in serverAddr {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(targetPort_);
    memcpy(&serverAddr.sin_addr.s_addr, target->h_addr, target->h_length);

    // Connect to Target Server
    if (connect(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("[!] Connect to target failed");
        close(clientSocket);
        close(serverSocket);
        return;
    }
    std::cout << "[+] Connection established: Client ↔ Proxy ↔ "
              << targetHost_ << ":" << targetPort_ << std::endl;

    // Start Bi-directional Forwarding
    std::cout << "[*] Starting data forwarding threads..." << std::endl;

    std::thread t1(&ProxyServer::forwardData, this, clientSocket, serverSocket, "Client → Server");
    std::thread t2(&ProxyServer::forwardData, this, serverSocket, clientSocket, "Server → Client");

    t1.join();
    t2.join();

    std::cout << "[*] Forwarding threads finished. Closing sockets..." << std::endl;
    close(clientSocket);
    close(serverSocket);
    std::cout << "[-] Connection closed." << std::endl;
}


void ProxyServer::forwardData(int sourceSocket, int destSocket, const std::string& direction) {
    char buffer[4096];
    ssize_t bytesRead;

    while ((bytesRead = recv(sourceSocket, buffer, sizeof(buffer), 0)) > 0) {
        std::string data(buffer, bytesRead);

        if (direction == "Client → Server") {
            std::cout << "[Intercept] Original Client Request:\n" << data << std::endl;

            // Find and replace Host header
            size_t hostPos = data.find("Host:");
            if (hostPos != std::string::npos) {
                size_t endOfLine = data.find("\r\n", hostPos);
                if (endOfLine != std::string::npos) {
                    std::string originalHostLine = data.substr(hostPos, endOfLine - hostPos);
                    std::string newHostLine = "Host: " + targetHost_;
                    data.replace(hostPos, originalHostLine.length(), newHostLine);
                    std::cout << "[Modify] Replaced Host Header → " << newHostLine << std::endl;
                }
            }
        }

        send(destSocket, data.c_str(), data.length(), 0);
        std::cout << "[Data] " << direction << " - " << data.length() << " bytes" << std::endl;
    }

    if (bytesRead == 0) {
        std::cout << "[*] " << direction << " connection closed by peer." << std::endl;
    } else if (bytesRead < 0) {
        perror(("[!] recv() failed in " + direction).c_str());
    }
}




/*
 *
| **Element**   | **Purpose**                                             |
| ------------- | ------------------------------------------------------- |
| `sockaddr_in` | Struct to hold IPv4 address + port                      |
| `AF_INET`     | Address family constant for IPv4                        |
| `INADDR_ANY`  | Binds socket to all local interfaces (0.0.0.0)          |
| `htons()`     | Converts port number to network byte order (big-endian) |

*/
