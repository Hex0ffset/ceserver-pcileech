#include "LinuxSocketWrapper.h"

#include <cstring>

#include <memory>

LinuxSocketWrapper::LinuxSocketWrapper(SOCKET socket) : socket_(socket) {};

std::shared_ptr<SocketWrapper> LinuxSocketWrapper::createSocket(int af, int type, int protocol)
{
	return std::make_shared<LinuxSocketWrapper>(::socket(af, type, protocol));
}

LinuxSocketWrapper::~LinuxSocketWrapper()
{
	if (socket_) ::close(socket_);
}

int LinuxSocketWrapper::recv(char* buffer, size_t size, int flags) const
{
	return ::recv(socket_, buffer, size, flags);
}

int LinuxSocketWrapper::recvNonBlocking(char * buffer, size_t size, int flags) const {
	return recv((char *) &buffer, size, MSG_DONTWAIT | flags);
}

int LinuxSocketWrapper::send(const char* buffer, size_t size, int flags) const
{
	return ::send(socket_, buffer, size, flags);
}

int LinuxSocketWrapper::setsockopt(int level, int optname, const char* optval, int optlen) const
{
	return ::setsockopt(socket_, level, optname, optval, optlen);
}

int LinuxSocketWrapper::bind(const struct sockaddr* name, int namelen) const
{
	return ::bind(socket_, name, namelen);
}

int LinuxSocketWrapper::listen(int backlog) const
{
	return ::listen(socket_, backlog);
}

int LinuxSocketWrapper::close() const
{
	return ::close(socket_);
}

std::shared_ptr<SocketWrapper> LinuxSocketWrapper::accept(sockaddr* addr, int* addrlen) const
{
	auto clientSocket = ::accept(socket_, addr, (socklen_t*)addrlen);

	return std::make_shared<LinuxSocketWrapper>(clientSocket);
}

bool LinuxSocketWrapper::valid() const
{
	if (socket_ < 0) return false;

	return true;
}

int LinuxSocketWrapper::lastError() const
{
	return errno;
}

char *LinuxSocketWrapper::lastErrorMsg() const {
	return strerror(errno);
}

int LinuxSocketWrapper::connect(sockaddr* addr, int addrlen) const {
	return ::connect(socket_, addr, addrlen);
}

int LinuxSocketWrapper::getsockname(sockaddr* addr, int* addrlen) const {
	return ::getsockname(socket_, addr, (socklen_t*)addrlen);
}
