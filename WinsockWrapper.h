#pragma once

#include <memory>

#include "SocketWrapper.h"
#include <Winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

class WinsockWrapper : public SocketWrapper
{
	SOCKET socket_;
public:
	WinsockWrapper(SOCKET socket);

	static std::shared_ptr<SocketWrapper> createSocket(int af, int type, int protocol);

	~WinsockWrapper();

	int recv(char* buffer, size_t size, int flags) const override;

	int recvNonBlocking(char *buffer, size_t size, int flags) const override;

	int send(const char* buffer, size_t size, int flags) const override;

	int setsockopt(int level, int optname, const char* optval, int optlen) const override;

	int bind(const struct sockaddr* name, int namelen) const override;

	int listen(int backlog) const override;

	int close() const override;

	std::shared_ptr<SocketWrapper> accept(sockaddr* addr, int* addrlen) const override;

	bool valid() const override;

	int lastError() const override;

	char* lastErrorMsg() const override;

	int connect(sockaddr* addr, int addrlen) const override;

	int getsockname(sockaddr* addr, int* addrlen) const override;
};
