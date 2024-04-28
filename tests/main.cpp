#include <gtest/gtest.h>
#include <absl/log/initialize.h>

#include <absl/log/globals.h>

#ifdef _WIN32
#include <WinSock2.h>
#endif

class Environment : public ::testing::Environment {
public:
#ifdef _WIN32
	WSADATA wsaData;
#endif

	void SetUp() override {
#ifdef _WIN32
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			std::cerr << "Failed to set up WinSock2. Tests related to sockets will likely fail" << std::endl;
		}
#endif
	}

	// Override this to define how to tear down the environment.
	void TearDown() override {
#ifdef _WIN32
		WSACleanup();
#endif
	}
};


int main(int argc, char **argv) {
	absl::InitializeLog();
	absl::SetGlobalVLogLevel(5);
	absl::SetStderrThreshold(absl::LogSeverity::kInfo);

	testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(new Environment());

	return RUN_ALL_TESTS();
}
