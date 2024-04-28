#include <gtest/gtest.h>
#include <gmock/gmock.h>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/tcp.h>
#endif

#include "../Server.h"
#include "mocks/Memory.h"
#include "mocks/SocketWrapper.h"

using ::testing::Return;
using ::testing::Pointee;
using ::testing::Eq;
using ::testing::SaveArgPointee;
using ::testing::_;

class ServerTest : public ::testing::Test {
protected:
    Server *server;
    std::shared_ptr<MockSocketWrapper> serverSocket = nullptr;
    std::shared_ptr<MockSocketWrapper> clientSocket = nullptr;
    std::shared_ptr<MockMemory> mockMemory = nullptr;
    struct sockaddr_in sockaddr_;

    void SetUp() override {
        serverSocket = std::reinterpret_pointer_cast<MockSocketWrapper>(
            MockSocketWrapper::createSocket(AF_INET, SOCK_STREAM, 0));
        clientSocket = std::reinterpret_pointer_cast<MockSocketWrapper>(
            MockSocketWrapper::createSocket(AF_INET, SOCK_STREAM, 0));
        mockMemory = std::make_shared<MockMemory>();

        sockaddr_.sin_family = AF_INET;
        sockaddr_.sin_addr.s_addr = INADDR_ANY;
        sockaddr_.sin_port = htons(0);

        server = new Server(sockaddr_, serverSocket, mockMemory.get());
    }

    void TearDown() override {
        server->stop();
        delete server;
    }
};

TEST_F(ServerTest, HappyPathStartup) {
    ON_CALL(*serverSocket, valid)
            .WillByDefault(Return(true));

    EXPECT_CALL(*serverSocket, setsockopt(SOL_SOCKET, SO_REUSEADDR, Pointee(Eq(1)), sizeof(int))).WillOnce(Return(0));

    EXPECT_CALL(*serverSocket, bind(::testing::_, sizeof(sockaddr_))).WillOnce(Return(0));

    ON_CALL(*serverSocket, accept(_, _)).WillByDefault(Return(nullptr));

    ASSERT_TRUE(server->init());

    EXPECT_CALL(*serverSocket, listen(32)).WillOnce(Return(0));
    ASSERT_TRUE(server->start());

    EXPECT_FALSE(server->start()); // already started, so should return false
}
