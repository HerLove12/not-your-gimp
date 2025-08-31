//
// Created by herlove on 07/05/25.
//

#ifndef PROXYSERVER_H
#define PROXYSERVER_H
#include <fstream>

#endif //PROXYSERVER_H

#pragma once
#include <string>

class ProxyServer {
public:
    explicit ProxyServer(int listenPort);
    void start();
    void stop();

private:
    bool running_ = false;
    int server_fd_ = -1;

    int listenPort_;
    std::ofstream logFile;

    void handleClient(int clientSocket);
    void forwardData(int fromSocket, int toSocket, const std::string& direction);
};
