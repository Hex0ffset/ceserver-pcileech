#include "Server.h"
#include "Client.h"
#include "Commands.h"
#include "SocketWrapper.h"
#include <iostream>

#include <absl/log/log.h>

#ifdef LINUX
#include <netinet/tcp.h>
#endif

Server::Server(struct sockaddr_in sockaddr_, std::shared_ptr<SocketWrapper> serverSocket,
               Memory *memory) : server(sockaddr_),
                                 serverSocket(serverSocket), memory(memory) {
};

Server::~Server() {
}

void Server::setSocketFactory(CreateSocketFunc func) {
    createSocket = func;
}

bool Server::init() {
    int reuseAddrVal = 1;
    int size = sizeof(reuseAddrVal);
    if (serverSocket->setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *) &reuseAddrVal, size) != 0) {
        LOG(ERROR) << "Failed to set SO_REUSEADDR on socket: " << serverSocket->lastError() << " (" << serverSocket->
                lastErrorMsg() << ")" << std::endl;
        return false;
    }

    if (serverSocket->bind((struct sockaddr *) &server, sizeof(server)) != 0) {
        LOG(ERROR) << "Bind failed with error code: " << serverSocket->lastError() << " (" << serverSocket->
                lastErrorMsg() << ")" << std::endl;
        return false;
    }

    return true;
}

bool Server::start() {
    if (serverThread != nullptr) {
        LOG(ERROR) << "Server has already been started!" << std::endl;
        return false;
    }

    if (serverSocket->listen(32) != 0) {
        LOG(ERROR) << "Bind failed with error code: " << serverSocket->lastError() << std::endl;
        return false;
    }
    LOG(INFO) << "Server listening on port " << ntohs(server.sin_port) << std::endl;

    serverThread = new std::thread([this]() {
        while (!bStop) {
            struct sockaddr_in client{};
            int clientSize = sizeof(client);
            auto clientSocket = serverSocket->accept((struct sockaddr *) &client, &clientSize);

            if (clientSocket == nullptr) { // unit tests will result in nullptr for example
                continue;
            }

            LOG(INFO) << "Connection accepted from " << Client::getClientAddress(client) << ":" << ntohs(client.sin_port);


            int tcpNoDelayValue = 1;
            if (clientSocket->setsockopt(IPPROTO_TCP, TCP_NODELAY, (char *) &tcpNoDelayValue, sizeof(int)) != 0) {
                LOG(ERROR) << "Failed to set flag TCP_NODELAY on socket (" << clientSocket->lastErrorMsg() <<
 "). Closing connection" <<
                        std::endl;
                clientSocket->close();
                continue;
            }

            clientThreads.push_back(new std::thread(&Server::handleClientConnection, this, client, clientSocket));
        }

        for (auto &thread: clientThreads) {
            if (thread->joinable()) thread->join();
            delete thread;
        }
    });

    return true;
}

void Server::stop() {
    bStop = true;
}

void Server::join() {
    if (serverThread != nullptr) {
        if (serverThread->joinable()) serverThread->join();
        delete serverThread;
    }
}

void Server::handleClientConnection(struct sockaddr_in clientInfo, std::shared_ptr<SocketWrapper> clientSocket) {
    Client client(clientInfo, clientSocket, memory, &sharedState);

    while (!bStop) {
        char buffer[1] = {0};
        int recvSize = clientSocket->recv(buffer, 1, MSG_WAITALL);

        if (recvSize < 0) {
            LOG(ERROR) << "recv failed while wating for next command: " << serverSocket->lastError() << " (" << serverSocket->lastErrorMsg() << ")";
            clientSocket->close();
            return;
        }

        if (recvSize == 0) continue;

        auto command = (unsigned char *) buffer;

        if (*command == CMD_TERMINATESERVER) {
            LOG(WARNING) << "Refusing to obey CMD_TERMINATESERVER" << std::endl;
        }

        if (!client.dispatchCommand(*command)) {
            LOG(ERROR) << "Closing socket as per dispatchCommand result";
            clientSocket->close();
            return;
        }
    }
}
