#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../LinuxSocketWrapper.h"

class MockLinuxSocketWrapper : public LinuxSocketWrapper {
public:
    MOCK_METHOD(int, send, (const char*, size_t, int), (const, override));
    MOCK_METHOD(int, recv, (char*, size_t, int), (const, override));
};

class SocketWrapperTest : public ::testing::Test {
protected:
    std::shared_ptr<MockLinuxSocketWrapper> socketWrapper;

    void SetUp() override {
        socketWrapper = std::reinterpret_pointer_cast<MockLinuxSocketWrapper>(
            MockLinuxSocketWrapper::createSocket(AF_INET, SOCK_STREAM, 0));
    }

    void TearDown() override {
        socketWrapper->close();
    }
};

TEST_F(SocketWrapperTest, ValidSocketCreation) {
    EXPECT_TRUE(socketWrapper->valid());
}

TEST_F(SocketWrapperTest, SendReceiveData) {
    auto serverSocket = socketWrapper;
    struct sockaddr_in server{};
    server.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);
    server.sin_port = htons(0);
    int addrlen = sizeof(server);

    EXPECT_EQ(serverSocket->bind((struct sockaddr *) &server, addrlen), 0);
    EXPECT_EQ(serverSocket->listen(32), 0);

    auto serverThreadMain = [&serverSocket]() {
        std::cout << "Waiting for test client to connect" << std::endl;

        struct sockaddr_in client{};
        int clientSize = sizeof(client);
        auto clientSocket = serverSocket->accept((struct sockaddr *) &client, &clientSize);

        std::cout << "Test client connected" << std::endl;

        clientSocket->send<int>(123, 0);
        clientSocket->close();
        serverSocket->close();
    };

    std::thread serverThread(serverThreadMain);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // give server time to listen

    addrlen = sizeof(server);
    EXPECT_EQ(serverSocket->getsockname((struct sockaddr *)&server, &addrlen), 0);
    std::cout << "Test server listening on port " << ntohs(server.sin_port) << std::endl;

    auto clientSocket = MockLinuxSocketWrapper::createSocket(AF_INET, SOCK_STREAM, 0);
    std::cout << "Test client connecting" << std::endl;

    struct sockaddr_in connectAddr{};
    connectAddr.sin_family = AF_INET;
    connectAddr.sin_port = server.sin_port;
    inet_pton(AF_INET, "127.0.0.1", &connectAddr.sin_addr);

    EXPECT_EQ(clientSocket->connect((struct sockaddr *)&connectAddr, addrlen), 0);

    auto actualRecvValue = clientSocket->recv<int>(MSG_WAITALL);
    EXPECT_EQ(actualRecvValue, 123);

    serverThread.join();
}