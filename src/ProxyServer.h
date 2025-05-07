//
// Created by herlove on 07/05/25.
//

#ifndef PROXYSERVER_H
#define PROXYSERVER_H

#endif //PROXYSERVER_H

#pragma once
#include <string>

class ProxyServer {
public:
    ProxyServer(int listenPort, const std::string& targetHost, int targetPort);
    void start();

private:
    int listenPort_;
    std::string targetHost_;
    int targetPort_;

    void handleClient(int clientSocket);
};
