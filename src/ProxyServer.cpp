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
    //create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        return;
    }

    sockaddr_in addr {}; // struct that holds ipv4 + port
    addr.sin_family = AF_INET; // ipv4
    addr.sin_addr.s_addr = INADDR_ANY; // bind to all interfaces 0.0.0.0
    addr.sin_port = htons(listenPort_); // format port number to big endiand

    //reuse adress/port without keeping the old connection open
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(server_fd);
        return;
    }

    //bind socket to address and port
    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        return;
    }

    //start listening
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
    std::cout << "[*] New client connected. Reading request...\n";

    // Step 1: Read HTTP Request Header
    std::string requestData;
    char buffer[4096];
    ssize_t bytesRead;

    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        requestData.append(buffer, bytesRead);
        if (requestData.find("\r\n\r\n") != std::string::npos) {
            break;  // End of headers
        }
    }

    if (requestData.empty()) {
        std::cerr << "[-] Failed to read request from client.\n";
        close(clientSocket);
        return;
    }

    std::cout << "[Intercept] Original Client Request:\n" << requestData << "\n";

    // Step 2: Extract Host Header
    std::string host;
    size_t hostPos = requestData.find("Host:");
    if (hostPos != std::string::npos) {
        size_t hostStart = hostPos + 5;  // Skip "Host:"
        size_t hostEnd = requestData.find("\r\n", hostStart);
        host = requestData.substr(hostStart, hostEnd - hostStart);
        // Trim spaces
        host.erase(0, host.find_first_not_of(" \t"));
        host.erase(host.find_last_not_of(" \t") + 1);
    } else {
        std::cerr << "[-] No Host header found.\n";
        close(clientSocket);
        return;
    }

    std::cout << "[+] Extracted Host: " << host << "\n";

    // Step 3: Resolve Target Hostname
    struct hostent* target = gethostbyname(host.c_str());
    if (target == nullptr) {
        std::cerr << "[-] Failed to resolve hostname: " << host << "\n";
        close(clientSocket);
        return;
    }

    std::cout << "[+] Resolved " << host << " to IP: " << inet_ntoa(*(struct in_addr*)target->h_addr) << "\n";

    // Step 4: Connect to Target Server
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Socket to target failed");
        close(clientSocket);
        return;
    }

    sockaddr_in serverAddr {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(80);  // Assuming HTTP only
    memcpy(&serverAddr.sin_addr.s_addr, target->h_addr, target->h_length);

    if (connect(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connect to target failed");
        close(clientSocket);
        close(serverSocket);
        return;
    }

    std::cout << "[+] Connection established: Client ↔ Proxy ↔ " << host << "\n";

    // Step 5: Forward the request to the real server
    send(serverSocket, requestData.c_str(), requestData.length(), 0);

    // Step 6: Start Bi-Directional Data Forwarding
    std::thread t1(&ProxyServer::forwardData, this, clientSocket, serverSocket, "Client → Server");
    std::thread t2(&ProxyServer::forwardData, this, serverSocket, clientSocket, "Server → Client");

    t1.join();
    t2.join();

    close(clientSocket);
    close(serverSocket);
    std::cout << "[-] Connection closed.\n";
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
