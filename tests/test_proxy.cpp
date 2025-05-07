// tests/test_proxy.cpp
#include <gtest/gtest.h>
#include "ProxyServer.h"

TEST(ProxyServerTest, CanConstruct) {
    ProxyServer proxy(8080, "example.com", 80);
    EXPECT_TRUE(true);
}
