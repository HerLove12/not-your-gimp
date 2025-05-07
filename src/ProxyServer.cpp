#include "ProxyServer.h"
#include <iostream>
#include <thread>
#include <unistd.h> // close()
#include <arpa/inet.h>

ProxyServer::ProxyServer(int listenPort, const std::string& targetHost, int targetPort)
    : listenPort_(listenPort), targetHost_(targetHost), targetPort_(targetPort) {}

void ProxyServer::start() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        return;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listenPort_);

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
    char buffer[4096];
    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead > 0) {
        send(clientSocket, buffer, bytesRead, 0);
    }
    close(clientSocket);
}
