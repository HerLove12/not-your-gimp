#include "ProxyServer.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <curl/curl.h>

// helper to catch curl response into std::string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class ProxyServerTest : public ::testing::Test {
protected:
    ProxyServer* proxy;
    std::thread serverThread;

    void SetUp() override {
        proxy = new ProxyServer(8080, "example.com", 80);
        serverThread = std::thread([this]() { proxy->start(); });

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        proxy->stop();
        if (serverThread.joinable()) {
            serverThread.join();
        }
        delete proxy;

        //need to sleep or the new proxy might not start properly, tried editing stop() method but it didn't work

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::string makeCurlRequest(const std::string& url) {
        CURL* curl;
        CURLcode res;
        std::string readBuffer;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_PROXY, "http://127.0.0.1:8080");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }
        return readBuffer;
    }
};

// connectivity test
TEST_F(ProxyServerTest, CanFetchThroughProxy) {
    std::string response = makeCurlRequest("http://example.com");
    ASSERT_FALSE(response.empty());
    ASSERT_NE(response.find("Example Domain"), std::string::npos);
}

// Injection test
TEST_F(ProxyServerTest, InjectsBannerInResponse) {
    std::string response = makeCurlRequest("http://example.com");
    ASSERT_NE(response.find("[GIMP proxy injection worked]"), std::string::npos);
}
