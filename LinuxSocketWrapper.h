#pragma once

#include <memory>

#include "SocketWrapper.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef int SOCKET;

class LinuxSocketWrapper : public SocketWrapper
{
	SOCKET socket_;
public:
	LinuxSocketWrapper(SOCKET socket);

	static std::shared_ptr<SocketWrapper> createSocket(int af, int type, int protocol);

	~LinuxSocketWrapper();

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
