#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <string>
#include <memory>
#include <unordered_map>
#include <variant>
#include <mutex>
#include <vector>
#include "SocketWrapper.h"
#include "Memory.h"
#include "ClientState.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#include <tlhelp32.h>
#endif

#ifdef LINUX
// https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot
#define TH32CS_SNAPMODULE 0x00000008
#define TH32CS_SNAPMODULE32 0x00000010
#define TH32CS_SNAPPROCESS 0x00000002
#define TH32CS_SNAPTHREAD 0x00000004
#endif

#ifdef _WIN32
#include <Windows.h>
#else
#define PAGE_NOACCESS           0x01
#define PAGE_READONLY           0x02
#define PAGE_READWRITE          0x04
#define PAGE_WRITECOPY          0x08
#define PAGE_EXECUTE            0x10
#define PAGE_EXECUTE_READ       0x20
#define PAGE_EXECUTE_READWRITE  0x40
#define PAGE_EXECUTE_WRITECOPY  0x80
#define PAGE_GUARD              0x100
#define PAGE_WRITECOMBINE       0x400
#define PAGE_NOCACHE            0x200

#define MEM_PRIVATE                 0x00020000
#define MEM_MAPPED                  0x00040000
#define MEM_IMAGE                   0x01000000
#endif

#ifdef LINUX
#include <sys/utsname.h>
typedef struct utsname SYSTEM_INFO;
#endif

#include <gtest/gtest_prod.h>

#define ABI_WINDOWS 0

class Client {
protected:
    friend class ClientTest;
    friend class MockClient;
    FRIEND_TEST(ClientTest, SendProcess32Entry);
    FRIEND_TEST(ClientTest, SendModule32Entry);
    FRIEND_TEST(ClientTest, SendProcess32EntryEndOfList);
    FRIEND_TEST(ClientTest, SendModule32EntryEndOfList);
    FRIEND_TEST(ClientTest, CreateProcessSnapshot);
    FRIEND_TEST(ClientTest, CreateModuleSnapshot);
    FRIEND_TEST(ClientTest, CreateThreadSnapshotEx);
    FRIEND_TEST(ClientTest, CreateModuleSnapshotEx);
    FRIEND_TEST(ClientTest, Process32First);
    FRIEND_TEST(ClientTest, Process32Next);
    FRIEND_TEST(ClientTest, Module32First);
    FRIEND_TEST(ClientTest, Module32Next);
    FRIEND_TEST(ClientTest, CloseHandle);
    FRIEND_TEST(ClientTest, OpenProcess);
    FRIEND_TEST(ClientTest, ReadProcessMemory);
    FRIEND_TEST(ClientTest, WriteProcessMemory);
    FRIEND_TEST(ClientTest, GetArchitectureFromMemoryModel);
    FRIEND_TEST(ClientTest, GetRegionInfo);
    FRIEND_TEST(ClientTest, SendVirtualQueryExResult);
    FRIEND_TEST(ClientTest, SendVirtualQueryExResultNull);
    FRIEND_TEST(ClientTest, VirtualQueryExFull);
    FRIEND_TEST(ClientTest, SendVirtualQueryExFullResult);
    FRIEND_TEST(ClientTest, SystemArchitecture);

    std::mutex mtx;

    struct sockaddr_in client;

    std::shared_ptr<SocketWrapper> clientSocket;

    std::string connectionName_;

    std::string threadId;

    Memory *memory;

    ClientState *state_;

    template<typename T>
    T read() const;

    template<typename T>
    T read(int flags) const;

    int read(char *buffer, size_t size) const;

    int read(char *buffer, size_t size, int flags) const;

    template<typename T>
    size_t send(T data) const;

    size_t send(const char *data, size_t size) const;

    size_t send(const char *data, size_t size, int flags) const;

    void handleUnknownCommand(unsigned char command) const;

    void handleKnownButUnimplementedCommand(unsigned char command) const;

    void handleCommandToolHelp32Snapshot();

    void handleCommandToolHelp32SnapshotEx();

    void handleCommandIterateProcessesNext(unsigned char command);

    void handleCommandIterateModulesNext(unsigned char command);

    void handleCommandCloseHandle();

    void handleCommandGetVersion();

    void handleCommandGetABI();

    void handleCommandOpenProcess();

    virtual void sendProcess32Entry(bool hasNext, int32_t processID, std::string name) const;

    virtual void sendModule32Entry(bool hasNext, uint64_t moduleBase, int32_t modulePart, int32_t memoryRegionSize,
                                   uint32_t fileOffset, std::string name) const;

    virtual void sendThread32Entry(int threadID) const;

    void handleCommandSetConnectionName();

    bool handleCommandReadProcessMemory();

    bool handleCommandWriteProcessMemory();

    bool handleCommandGetArchitecture();

    void handleCommandStartDebug() const;

    void handleCommandGetOptions() const;

    virtual unsigned char getArchitectureFromMemoryModel(MemoryModel model) const;

    void handleCommandGetSymbolListFromFile();

    bool handleCommandGetRegionInfo(unsigned char command);

    bool handleCommandVirtualQueryExFull();

    std::string vadProtectionToHumanReadable(PVMMDLL_MAP_VADENTRY vadEntry) const;

    int32_t vadProtectionToWin32Protection(PVMMDLL_MAP_VADENTRY vadEntry) const;

    int32_t vadTypeToWin32Type(PVMMDLL_MAP_VADENTRY vadEntry) const;

    virtual void sendVirtualQueryExResult(MemoryRegion *region) const;

    virtual void sendVirtualQueryExFullResult(MemoryRegion *region) const;

    virtual ClientState *state() const;

public:
    static inline std::string VERSION_STRING = "CHEATENGINE Network 2.3";
    static inline int32_t CE_SERVER_VERSION = 6;

    Client(struct sockaddr_in clientInfo, std::shared_ptr<SocketWrapper> clientSocket, Memory *memory,
           ClientState *state);

    ~Client();

    std::string getClientAddress() const;

    static std::string getClientAddress(struct sockaddr_in client);

    bool dispatchCommand(unsigned char command);

    std::string connectionName() const;
};

static_assert(sizeof(int) == sizeof(int32_t));
