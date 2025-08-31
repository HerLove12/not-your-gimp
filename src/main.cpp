#include "ProxyServer.h"

int main() {
    ProxyServer proxy(8080);
    proxy.start();
    return 0;
}

// curl -v -x http://127.0.0.1:8080 http://example.com
// -x specifies the proxy server