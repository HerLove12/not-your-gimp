#include "ProxyServer.h"

#include <cstring>
#include <iostream>
#include <netdb.h>
#include <thread>
#include <unistd.h> // system commands close() sockets
#include <arpa/inet.h> // network functions like socket(), bind(), listen(), accept()
#include <fstream>

//std::ofstream logFile("proxy.log", std::ios::app); MOVED TO CONSTRUCTOR


ProxyServer::ProxyServer(int listenPort, const std::string& targetHost, int targetPort)
    : listenPort_(listenPort), targetHost_(targetHost), targetPort_(targetPort), logFile("proxy.log", std::ios::app)
{
    if (!logFile.is_open())
    {
        std::cerr << "[-] Failed to open log file!" << std::endl;
    }
}

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

    // Step 1: Read initial data from client
    std::string requestData;
    char buffer[4096];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (bytesRead <= 0) {
        std::cerr << "[-] Failed to read request from client.\n";
        close(clientSocket);
        return;
    }

    requestData.append(buffer, bytesRead);

    //std::cout << "[Intercept] Original Client Request:\n" << requestData << "\n";

    // Step 2: Extract Host Header
    std::string host;
    size_t hostPos = requestData.find("Host:");
    if (hostPos != std::string::npos) {
        size_t hostStart = hostPos + 5;
        size_t hostEnd = requestData.find("\r\n", hostStart);
        host = requestData.substr(hostStart, hostEnd - hostStart);
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

    std::cout << "[+] Resolved " << host << " to IP: "
              << inet_ntoa(*(struct in_addr*)target->h_addr) << "\n";

    // Step 4: Connect to Target Server
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Socket to target failed");
        close(clientSocket);
        return;
    }

    sockaddr_in serverAddr {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(80);
    memcpy(&serverAddr.sin_addr.s_addr, target->h_addr, target->h_length);

    if (connect(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connect to target failed");
        close(clientSocket);
        close(serverSocket);
        return;
    }

    std::cout << "[+] Connection established: Client ↔ Proxy ↔ " << host << "\n";

    // Step 5: Log and forward the first chunk immediately
    logFile << "[Client Request]\n" << requestData << "\n---\n";
    logFile.flush(); // ensure it appears in proxy.log
///*
    //allow browser view
    //disable server compression by forcing Accept-Encoding: identity
    size_t aePos = requestData.find("Accept-Encoding:");
    if (aePos != std::string::npos) {
        size_t endOfLine = requestData.find("\r\n", aePos);
        if (endOfLine != std::string::npos) {
            requestData.replace(aePos, endOfLine - aePos, "Accept-Encoding: identity");
        }
    } else {
        // If header is missing, add it after first line (after method + path)
        size_t firstLineEnd = requestData.find("\r\n");
        if (firstLineEnd != std::string::npos) {
            requestData.insert(firstLineEnd + 2, "Accept-Encoding: identity\r\n");
        }
    }

//*/
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
    const size_t bufferSize = 8192; // larger buffer for efficiency
    char buffer[bufferSize];
    ssize_t bytesRead;

    if (direction == "Client → Server") {
        // Forward client request, modifying Host header
        std::string requestData;
        while ((bytesRead = recv(sourceSocket, buffer, bufferSize, 0)) > 0) {
            std::string chunk(buffer, bytesRead);

            // Modify Host header dynamically
            size_t hostPos = chunk.find("Host:");
            if (hostPos != std::string::npos) {
                size_t endOfLine = chunk.find("\r\n", hostPos);
                if (endOfLine != std::string::npos) {
                    std::string originalHostLine = chunk.substr(hostPos, endOfLine - hostPos);
                    std::string newHostLine = "Host: " + targetHost_;
                    chunk.replace(hostPos, originalHostLine.length(), newHostLine);
                    std::cout << "[Modify] Replaced Host Header → " << newHostLine << std::endl;
                }
            }

            // Remove If-None-Match and If-Modified-Since to force fresh response
            size_t etagPos = requestData.find("If-None-Match:");
            if (etagPos != std::string::npos) {
                size_t endOfLine = requestData.find("\r\n", etagPos);
                requestData.erase(etagPos, endOfLine - etagPos + 2);
            }

            size_t imsPos = requestData.find("If-Modified-Since:");
            if (imsPos != std::string::npos) {
                size_t endOfLine = requestData.find("\r\n", imsPos);
                requestData.erase(imsPos, endOfLine - imsPos + 2);
            }

            // Force identity encoding to disable compression
            size_t aePos = chunk.find("Accept-Encoding:");
            if (aePos != std::string::npos) {
                size_t endOfLine = chunk.find("\r\n", aePos);
                if (endOfLine != std::string::npos) {
                    chunk.replace(aePos, endOfLine - aePos, "Accept-Encoding: identity");
                }
            }

            requestData.append(chunk);
            send(destSocket, chunk.c_str(), chunk.length(), 0);
            std::cout << "[Data] " << direction << " - " << chunk.length() << " bytes" << std::endl;
        }

        if (bytesRead <= 0)
            std::cout << "[*] " << direction << " connection closed by peer." << std::endl;

        logFile << "[Client Request]\n" << requestData << "\n---\n";
        logFile.flush();
    }
    else if (direction == "Server → Client") {
        // Accumulate full response first
        std::string fullResponse;
        while ((bytesRead = recv(sourceSocket, buffer, bufferSize, 0)) > 0) {
            fullResponse.append(buffer, bytesRead);
        }

        if (bytesRead < 0) {
            perror(("[!] recv() failed in " + direction).c_str());
        }

        // Disable chunked transfer encoding if present
        size_t tePos = fullResponse.find("Transfer-Encoding: chunked");
        if (tePos != std::string::npos) {
            size_t endOfLine = fullResponse.find("\r\n", tePos);
            fullResponse.erase(tePos, endOfLine - tePos + 2); // remove header line
        }

        // Inject banner before </body>
        size_t bodyPos = fullResponse.find("</body>");
        if (bodyPos != std::string::npos) {
            std::string injection = "<h1 style='color:red;'>[GIMP proxy injection worked]</h1>";
            fullResponse.insert(bodyPos, injection);
            std::cout << "[Modify] Injected banner into response.\n";

            // Update Content-Length header if present
            size_t clPos = fullResponse.find("Content-Length:");
            if (clPos != std::string::npos) {
                size_t clEnd = fullResponse.find("\r\n", clPos);
                size_t headerEnd = fullResponse.find("\r\n\r\n");
                size_t bodyLength = fullResponse.length() - headerEnd - 4;
                std::string newCl = "Content-Length: " + std::to_string(bodyLength);
                fullResponse.replace(clPos, clEnd - clPos, newCl);
            }
        }

        logFile << "[Server Response]\n" << fullResponse << "\n---\n";
        logFile.flush();

        // Send modified response
        send(destSocket, fullResponse.c_str(), fullResponse.length(), 0);
        std::cout << "[Data] " << direction << " - " << fullResponse.length() << " bytes" << std::endl;

        std::cout << "[*] " << direction << " connection closed by peer." << std::endl;
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
