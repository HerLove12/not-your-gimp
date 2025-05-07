#include "ProxyServer.h"

int main() {
    ProxyServer proxy(8080, "example.com", 80);
    proxy.start();
    return 0;
}
