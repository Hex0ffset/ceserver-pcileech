#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../SocketWrapper.h"

class MockSocketWrapper : public SocketWrapper
{
public:
    MOCK_METHOD(int, send, (const char*, size_t, int), (const, override));

    MOCK_METHOD(int, recv, (char*, size_t, int), (const, override));

    MOCK_METHOD(int, recvNonBlocking, (char*, size_t, int), (const, override));

    static std::shared_ptr<SocketWrapper> createSocket(int af, int type, int flags)
    {
        return std::make_shared<MockSocketWrapper>();
    }

    MOCK_METHOD(int, setsockopt, (int, int, const char*, int), (const, override));

    MOCK_METHOD(int, bind, (const struct sockaddr* name, int namelen), (const, override));

    MOCK_METHOD(int, listen, (int), (const, override));

    MOCK_METHOD(int, close, (), (const, override));

    MOCK_METHOD(std::shared_ptr<SocketWrapper>, accept, (sockaddr*, int*), (const, override));

    MOCK_METHOD(bool, valid, (), (const, override));

    MOCK_METHOD(int, lastError, (), (const, override));

    MOCK_METHOD(char*, lastErrorMsg, (), (const, override));

	MOCK_METHOD(int,  connect, (sockaddr* addr, int addrlen), (const, override));

    MOCK_METHOD(int, getsockname, (sockaddr* addr, int* addrlen), (const, override));
};