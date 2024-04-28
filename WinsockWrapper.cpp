#include "WinsockWrapper.h"

#include <memory>

WinsockWrapper::WinsockWrapper(SOCKET socket) : socket_(socket) {
};

std::shared_ptr<SocketWrapper> WinsockWrapper::createSocket(int af, int type, int protocol) {
    return std::make_shared<WinsockWrapper>(::socket(af, type, protocol));
}

WinsockWrapper::~WinsockWrapper() {
    if (socket_) closesocket(socket_);
}

int WinsockWrapper::recv(char *buffer, size_t size, int flags) const {
    return ::recv(socket_, buffer, size, flags);
}

int WinsockWrapper::recvNonBlocking(char * buffer, size_t size, int flags) const {
    u_long mode = 1;
    ioctlsocket(socket_,FIONBIO,&mode);
    return recv(buffer, size, flags);
}

int WinsockWrapper::send(const char *buffer, size_t size, int flags) const {
    return ::send(socket_, buffer, size, flags);
}

int WinsockWrapper::setsockopt(int level, int optname, const char *optval, int optlen) const {
    return ::setsockopt(socket_, level, optname, optval, optlen);
}

int WinsockWrapper::bind(const struct sockaddr *name, int namelen) const {
    return ::bind(socket_, name, namelen);
}

int WinsockWrapper::listen(int backlog) const {
    return ::listen(socket_, backlog);
}

int WinsockWrapper::close() const {
    return ::closesocket(socket_);
}

std::shared_ptr<SocketWrapper> WinsockWrapper::accept(sockaddr *addr, int *addrlen) const {
    auto clientSocket = ::accept(socket_, addr, addrlen);

    return std::make_shared<WinsockWrapper>(clientSocket);
}

bool WinsockWrapper::valid() const {
    if (socket_ == INVALID_SOCKET) return false;

    return true;
}

int WinsockWrapper::lastError() const {
    return WSAGetLastError();
}

char *WinsockWrapper::lastErrorMsg() const {
    static char message[256] = {0};
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        0, WSAGetLastError(), 0, message, 256, 0);
    char *nl = strrchr(message, '\n');
    if (nl) *nl = 0;
    return message;
}

int WinsockWrapper::connect(sockaddr* addr, int addrlen) const {
    return ::connect(socket_, addr, addrlen);
}

int WinsockWrapper::getsockname(sockaddr* addr, int* addrlen) const {
    return ::getsockname(socket_, addr, addrlen);
}
