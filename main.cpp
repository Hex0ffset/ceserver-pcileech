#include "Server.h"
#include "Memory.h"
#include <iostream>
#include <filesystem>
#include <absl/log/initialize.h>
#include <absl/log/globals.h>
#include <absl/flags/usage.h>
#include <absl/flags/flag.h>
#include <absl/flags/parse.h>

#ifdef _WIN32
#include "WinsockWrapper.h"
#else
#include "LinuxSocketWrapper.h"
#endif

ABSL_FLAG(uint32_t, port, 52736, "Port for the server to listen on");

int main(int argc, char** argv) {
    absl::SetProgramUsageMessage("A Cheat Engine server using PCILeech");
    absl::ParseCommandLine(argc, argv);
    absl::SetStderrThreshold(absl::LogSeverity::kInfo);
    absl::InitializeLog();

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock: " << WSAGetLastError() << std::endl;
        return EXIT_FAILURE;
    }
    auto serverSocket = WinsockWrapper::createSocket(AF_INET, SOCK_STREAM, 0);
#else
	auto serverSocket = LinuxSocketWrapper::createSocket(PF_INET, SOCK_STREAM, 0);
#endif

    auto mmapPath = std::filesystem::current_path() / "mmap.txt";
    auto pcileech = std::make_shared<PCILeechWrapper>();
    Memory memory(mmapPath, pcileech);
    if (!memory.init()) {
        std::cerr << "Failed to init PCI leech memory adapter" << std::endl;
        return EXIT_FAILURE;
    }

    struct sockaddr_in sockaddr_{};
    sockaddr_.sin_family = AF_INET;
    sockaddr_.sin_addr.s_addr = INADDR_ANY;
    sockaddr_.sin_port = htons(absl::GetFlag(FLAGS_port));

    Server server(sockaddr_, serverSocket, &memory);
    server.init();
    if (!server.start()) return 1;

    server.join();

#ifdef _WIN32
    WSACleanup();
#endif

    return EXIT_SUCCESS;
}
