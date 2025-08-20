// tests/test_proxy.cpp
#include <gtest/gtest.h>
#include "ProxyServer.h"

class ProxyServerTest : public ::testing::Test {
protected:
    ProxyServer proxy{8080, "example.com", 80}; // test instance
};

TEST_F(ProxyServerTest, ExtractHostHeader) {
    std::string request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
    size_t hostPos = request.find("Host:");
    ASSERT_NE(hostPos, std::string::npos);

    size_t hostStart = hostPos + 5;
    size_t hostEnd = request.find("\r\n", hostStart);
    std::string host = request.substr(hostStart, hostEnd - hostStart);

    // Trim
    host.erase(0, host.find_first_not_of(" \t"));
    host.erase(host.find_last_not_of(" \t") + 1);

    EXPECT_EQ(host, "example.com");
}

TEST_F(ProxyServerTest, ForceIdentityEncoding) {
    std::string req = "GET / HTTP/1.1\r\nHost: test.com\r\nAccept-Encoding: gzip\r\n\r\n";

    size_t aePos = req.find("Accept-Encoding:");
    ASSERT_NE(aePos, std::string::npos);

    size_t endOfLine = req.find("\r\n", aePos);
    req.replace(aePos, endOfLine - aePos, "Accept-Encoding: identity");

    EXPECT_NE(req.find("identity"), std::string::npos);
}

TEST_F(ProxyServerTest, InjectsIntoResponse) {
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                       "<html><body>Hello world</body></html>";

    size_t pos = resp.find("</body>");
    ASSERT_NE(pos, std::string::npos);

    resp.insert(pos, "<h1 style='color:red;'>[Injected]</h1>");

    EXPECT_NE(resp.find("[Injected]"), std::string::npos);
}
