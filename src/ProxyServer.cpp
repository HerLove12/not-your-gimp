#include "ProxyServer.h"

#include <cstring>
#include <iostream>
#include <netdb.h>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>

ProxyServer::ProxyServer(int listenPort)
    : listenPort_(listenPort), logFile("proxy.log", std::ios::app)
{
    if (!logFile.is_open()) {
        std::cerr << "[-] Failed to open log file!" << std::endl;
    }
}

void ProxyServer::start() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        perror("Socket failed");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listenPort_);

    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd_);
        return;
    }

    if (bind(server_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(server_fd_);
        return;
    }

    if (listen(server_fd_, 5) < 0) {
        perror("Listen failed");
        close(server_fd_);
        return;
    }

    running_ = true;
    std::cout << "[*] Listening on port " << listenPort_ << std::endl;

    while (running_) {
        int clientSocket = accept(server_fd_, nullptr, nullptr);
        if (clientSocket < 0) {
            if (!running_) break;
            perror("Accept failed");
            continue;
        }

        std::cout << "[*] Client connected, spawning handler thread." << std::endl;
        std::thread(&ProxyServer::handleClient, this, clientSocket).detach();
    }

    close(server_fd_);
    server_fd_ = -1;
    std::cout << "[*] Proxy stopped listening." << std::endl;
}

void ProxyServer::stop() {
    running_ = false;
    if (server_fd_ != -1) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }
    logFile.close();
    std::cout << "[*] Proxy stopped." << std::endl;
}

void ProxyServer::handleClient(int clientSocket) {
    std::cout << "[*] Handling new client..." << std::endl;

    char buffer[4096];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead <= 0) {
        close(clientSocket);
        std::cout << "[-] Failed to read request from client." << std::endl;
        return;
    }

    std::string request(buffer, bytesRead);

    ///*
    size_t etagPos = request.find("If-None-Match:");
    if (etagPos != std::string::npos) {
        size_t endLine = request.find("\r\n", etagPos);
        request.erase(etagPos, endLine - etagPos + 2);
    }

    size_t imsPos = request.find("If-Modified-Since:");
    if (imsPos != std::string::npos) {
        size_t endLine = request.find("\r\n", imsPos);
        request.erase(imsPos, endLine - imsPos + 2);
    }
    //*/

    logFile << "[request]\n" << request << "\n";
    logFile.flush();

    std::string host;
    size_t hostPos = request.find("Host:");
    if (hostPos != std::string::npos) {
        size_t hostEnd = request.find("\r\n", hostPos);
        host = request.substr(hostPos + 5, hostEnd - (hostPos + 5));
        while (!host.empty() && (host[0] == ' ' || host[0] == '\t')) host.erase(0,1);
    }

    if (host.empty()) {
        close(clientSocket);
        std::cout << "[-] No Host header found, closing client." << std::endl;
        return;
    }

    std::cout << "[+] Target host: " << host << std::endl;

    hostent* target = gethostbyname(host.c_str());
    if (!target) {
        close(clientSocket);
        std::cout << "[-] Failed to resolve host." << std::endl;
        return;
    }

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        close(clientSocket);
        std::cout << "[-] Failed to create server socket." << std::endl;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(80);
    memcpy(&serverAddr.sin_addr.s_addr, target->h_addr, target->h_length);

    if (connect(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connect failed");
        close(clientSocket);
        close(serverSocket);
        return;
    }

    std::cout << "[*] Connected to server " << host << std::endl;

    size_t aePos = request.find("Accept-Encoding:");
    if (aePos != std::string::npos) {
        size_t endLine = request.find("\r\n", aePos);
        request.replace(aePos, endLine - aePos, "Accept-Encoding: identity");
    } else {
        size_t firstLineEnd = request.find("\r\n");
        request.insert(firstLineEnd + 2, "Accept-Encoding: identity\r\n");
    }

    send(serverSocket, request.c_str(), request.size(), 0);

    std::thread t1(&ProxyServer::forwardData, this, clientSocket, serverSocket, "Client → Server");
    std::thread t2(&ProxyServer::forwardData, this, serverSocket, clientSocket, "Server → Client");

    t1.join();
    t2.join();

    close(clientSocket);
    close(serverSocket);
    std::cout << "[*] Client connection closed." << std::endl;
}

void ProxyServer::forwardData(int fromSocket, int toSocket, const std::string& direction) {
    const size_t bufSize = 8192;
    char buffer[bufSize];
    ssize_t bytesRead;

    if (direction == "Server → Client") {
        std::string response;
        while (response.find("\r\n\r\n") == std::string::npos &&
               (bytesRead = recv(fromSocket, buffer, bufSize, 0)) > 0) {
            response.append(buffer, bytesRead);
        }
        if (response.empty()) return;

        size_t headerEnd = response.find("\r\n\r\n");
        std::string headers = response.substr(0, headerEnd + 4);
        std::string body = response.substr(headerEnd + 4);

        size_t clPos = headers.find("Content-Length:");
        if (clPos != std::string::npos) {
            size_t clEnd = headers.find("\r\n", clPos);
            int contentLen = std::stoi(headers.substr(clPos + 15, clEnd - (clPos + 15)));
            while ((int)body.size() < contentLen && (bytesRead = recv(fromSocket, buffer, bufSize, 0)) > 0) {
                body.append(buffer, bytesRead);
            }

            size_t injectPos = body.find("</body>");
            if (injectPos != std::string::npos) {
                std::string injection = "<h1 style='color:red;'>[GIMP proxy injection worked]</h1>";
                body.insert(injectPos, injection);
                int newLen = body.size();
                headers.replace(clPos, clEnd - clPos, "Content-Length: " + std::to_string(newLen));
                std::cout << "[+] Injected banner into response" << std::endl;
            }
        }

        std::string finalResp = headers + body;
        send(toSocket, finalResp.c_str(), finalResp.size(), 0);
        logFile << "[response]\n" << finalResp << "\n";
        logFile.flush();
        std::cout << "[*] Response sent to client." << std::endl;
    } else {
        while ((bytesRead = recv(fromSocket, buffer, bufSize, 0)) > 0) {
            send(toSocket, buffer, bytesRead, 0);
            logFile << "[request]\n" << std::string(buffer, bytesRead) << "\n";
            logFile.flush();
        }
        std::cout << "[*] Client request forwarded." << std::endl;
    }
}
