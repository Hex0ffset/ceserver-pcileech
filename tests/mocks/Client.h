#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../Client.h"
#include "../../ClientState.h"

class MockClient : public Client {
public:
    MockClient(struct sockaddr_in clientInfo, std::shared_ptr<SocketWrapper> clientSocket, Memory *memory, ClientState* state) : Client(
        client, clientSocket, memory, state) {
    };

    MOCK_METHOD(void, sendProcess32Entry, (bool, int32_t, std::string), (const, override));

    MOCK_METHOD(void, sendModule32Entry, (bool, uint64_t, int32_t, int32_t, uint32_t, std::string), (const, override));

    MOCK_METHOD(void, sendThread32Entry, (int), (const, override));

    MOCK_METHOD(unsigned char, getArchitectureFromMemoryModel, (MemoryModel), (const, override));

    MOCK_METHOD(void, sendVirtualQueryExResult, (MemoryRegion *), (const, override));

    MOCK_METHOD(void, sendVirtualQueryExFullResult, (MemoryRegion *), (const, override));
};
