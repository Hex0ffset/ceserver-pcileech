#pragma once

#include <memory>

typedef class std::shared_ptr<class SocketWrapper> (*CreateSocketFunc)(int, int, int);

class SocketWrapper {
public:
	SocketWrapper() {
	};

	virtual ~SocketWrapper() {
	};

	static std::shared_ptr<SocketWrapper> createSocket(int af, int type, int protocol);

	template<typename T>
	T recv() const {
		size_t recvSize;
		return recv<T>(&recvSize, 0);
	}

	template<typename T>
	T recv(int flags = 0) const {
		size_t recvSize;
		return recv<T>(&recvSize, flags);
	}

	template<typename T>
	T recv(size_t* recvSize, int flags = 0) const {
		T data{};

		*recvSize = recv((char *) &data, sizeof(T), flags);

		return data;
	}

	virtual int recvNonBlocking(char *buffer, size_t size, int flags) const = 0;

	virtual int recv(char *buffer, size_t size, int flags) const = 0;

	template<typename T>
	size_t send(T data, int flags) const {
		return send((char *) &data, sizeof(T), flags);
	}

	virtual int send(const char *buffer, size_t size, int flags) const = 0;

	virtual int setsockopt(int level, int optname, const char *optval, int optlen) const = 0;

	virtual int bind(const struct sockaddr *name, int namelen) const = 0;

	virtual int listen(int backlog) const = 0;

	virtual int connect(sockaddr* addr, int addrlen) const = 0;

	virtual int close() const = 0;

	virtual std::shared_ptr<SocketWrapper> accept(sockaddr *addr, int *addrlen) const = 0;

	virtual bool valid() const = 0;

	virtual int lastError() const = 0;

	virtual char* lastErrorMsg() const = 0;

	virtual int getsockname(sockaddr* addr, int* addrlen) const = 0;
};
