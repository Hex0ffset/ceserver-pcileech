#pragma once

#include <vector>
#include <thread>
#include "SocketWrapper.h"
#include <memory>
#include "Memory.h"
#include "ClientState.h"

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <netinet/in.h>
#endif

class Server
{
private:
	CreateSocketFunc createSocket = nullptr;
	Memory* memory;
	std::shared_ptr<SocketWrapper> serverSocket = nullptr;
	struct sockaddr_in server;
	bool bStop = false;
	std::thread* serverThread = nullptr;
	std::vector<std::thread*> clientThreads;

	ClientState sharedState;

	void handleClientConnection(struct sockaddr_in clientInfo, std::shared_ptr<SocketWrapper> clientSocket);

public:
	Server(struct sockaddr_in server, std::shared_ptr<SocketWrapper> serverSocket, Memory* memory);

	~Server();

	void setSocketFactory(CreateSocketFunc func);

	std::shared_ptr<SocketWrapper> socket();

	bool init();

	bool start();

	void stop();

	void join();
};