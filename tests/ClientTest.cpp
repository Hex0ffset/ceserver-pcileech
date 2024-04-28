#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../Client.h"
#include "../Commands.h"
#include "mocks/SocketWrapper.h"
#include "../Memory.h"
#include "mocks/Memory.h"
#include "mocks/Client.h"

using ::testing::Sequence;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Eq;
using ::testing::Return;
using ::testing::SaveArgPointee;
using ::testing::SetArgPointee;
using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArrayArgument;
using ::testing::Invoke;

class ClientTest : public ::testing::Test {
protected:
    Client *client;
    MockClient *mockClient;
    ClientState *clientState;
    struct sockaddr_in clientInfo;
    std::shared_ptr<MockSocketWrapper> socket = nullptr;
    MockMemory *memory = nullptr;

    void SetUp() override {
        memory = new MockMemory();
        socket = std::reinterpret_pointer_cast<MockSocketWrapper>(
            MockSocketWrapper::createSocket(AF_INET, SOCK_STREAM, 0));
        clientState = new ClientState();
        client = new Client(clientInfo, socket, memory, clientState);
        mockClient = new MockClient(clientInfo, socket, memory, clientState);
    }

    void TearDown() override {
        delete client;
        delete mockClient;
        delete memory;
        delete clientState;
    }
};

TEST_F(ClientTest, HandleKnownButUnimplementedCommandGracefully) {
    EXPECT_FALSE(client->dispatchCommand(-1));
}

TEST_F(ClientTest, GetVersion) {
    auto expectedVersion = CeVersion{};
    expectedVersion.version = Client::CE_SERVER_VERSION;
    expectedVersion.stringsize = Client::VERSION_STRING.size();

    auto actualVersion = CeVersion{};
    char actualVersionString[1024];

    EXPECT_CALL(*socket, send(_, sizeof(CeVersion), 0))
            .WillOnce(DoAll(Invoke([&actualVersion](const char *buffer, size_t size, int flags) {
                memcpy(&actualVersion, buffer, size);
                return size;
            }), Return(sizeof(CeVersion))));
    EXPECT_CALL(*socket, send(_, Client::VERSION_STRING.size(), 0))
            .WillOnce(DoAll(Invoke([&actualVersionString](const char *buffer, size_t size, int flags) {
                memcpy(actualVersionString, buffer, size);
                actualVersionString[size] = 0;
                return size;
            }), Return(sizeof(CeVersion))));

    // Now dispatch the command
    EXPECT_TRUE(client->dispatchCommand(CMD_GETVERSION));

    EXPECT_EQ(actualVersion.version, expectedVersion.version);
    EXPECT_EQ(actualVersion.stringsize, expectedVersion.stringsize);
    EXPECT_STREQ(actualVersionString, Client::VERSION_STRING.c_str());
}

TEST_F(ClientTest, GetABI) {
    EXPECT_CALL(*socket, send(Pointee(Eq(0)), sizeof(unsigned char), 0));

    EXPECT_TRUE(client->dispatchCommand(CMD_GETABI));
}

TEST_F(ClientTest, CloseConnection) {
    // false is used to signal the socket should be closed
    EXPECT_CALL(*socket, send).Times(0);
    EXPECT_FALSE(client->dispatchCommand(CMD_CLOSECONNECTION));
}

TEST_F(ClientTest, TerminateServer) {
    // false is used to signal the socket should be closed
    // the server code handles the actual termination
    EXPECT_CALL(*socket, send).Times(0);
    EXPECT_FALSE(client->dispatchCommand(CMD_TERMINATESERVER));
}

TEST_F(ClientTest, SetConnectionName) {
    std::string newConnectionName = "my connection";
    auto size = newConnectionName.size();
    Sequence seq;

    EXPECT_STREQ(client->connectionName().c_str(), "");

    EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL)) // reading out the length of the string
            .InSequence(seq)
            .WillOnce(DoAll(SetArgPointee<0>(size), Return(sizeof(uint32_t))));

    EXPECT_CALL(*socket, recv(Pointee(_), size, MSG_WAITALL)) // reading string itself
            .InSequence(seq)
            .WillOnce(DoAll(Invoke([&newConnectionName](char *buffer, size_t size, int flags) {
                memcpy(buffer, newConnectionName.c_str(), size);
            }), Return(size)));

    EXPECT_CALL(*socket, send).Times(0);
    EXPECT_TRUE(client->dispatchCommand(CMD_SET_CONNECTION_NAME));

    EXPECT_STREQ(client->connectionName().c_str(), newConnectionName.c_str());
}

TEST_F(ClientTest, SendProcess32Entry) {
    std::string processName = "explorer.exe";
    int32_t processID = 123;

    auto expectedEntry = CeProcessEntry{};
    expectedEntry.result = 1;
    expectedEntry.pid = processID;
    expectedEntry.processnamesize = processName.size();

    auto actualEntry = CeProcessEntry{};
    char actualProcessName[1024];
    //
    {
        InSequence seq;
        EXPECT_CALL(*socket, send(_, sizeof(CeProcessEntry), 0))
                .WillOnce(Invoke([&actualEntry](const char *buffer, size_t size, int flags) {
                    memcpy(&actualEntry, buffer, size);
                    return size;
                }));
        EXPECT_CALL(*socket, send(_, processName.size(), 0))
                .WillOnce(Invoke([&actualProcessName](const char *buffer, size_t size, int flags) {
                    memcpy(actualProcessName, buffer, size);
                    actualProcessName[size] = 0;
                    return size;
                }));
    }

    client->sendProcess32Entry(true, processID, processName);
    EXPECT_EQ(actualEntry.result, expectedEntry.result);
    EXPECT_EQ(actualEntry.pid, expectedEntry.pid);
    EXPECT_EQ(actualEntry.processnamesize, expectedEntry.processnamesize);
    EXPECT_STREQ(actualProcessName, processName.c_str());
}

TEST_F(ClientTest, SendProcess32EntryEndOfList) {
    auto expectedEntry = CeProcessEntry{};
    expectedEntry.result = 0;
    expectedEntry.pid = 0;
    expectedEntry.processnamesize = 0;

    auto actualEntry = CeProcessEntry{};

    EXPECT_CALL(*socket, send(_, sizeof(CeProcessEntry), 0))
            .WillOnce(Invoke([&actualEntry](const char *buffer, size_t size, int flags) {
                memcpy(&actualEntry, buffer, size);
                return size;
            }));
    // nothing on the wire for empty process name

    client->sendProcess32Entry(false, 0, "");
    EXPECT_EQ(actualEntry.result, expectedEntry.result);
    EXPECT_EQ(actualEntry.pid, expectedEntry.pid);
    EXPECT_EQ(actualEntry.processnamesize, expectedEntry.processnamesize);
}

TEST_F(ClientTest, SendModule32Entry) {
    std::string moduleName = "chrome.exe";
    int64_t moduleBase = 123;
    int32_t modulePart = 5;
    int32_t memoryRegionSize = 127;
    uint32_t fileOffset = 111;

    auto expectedEntry = CeModuleEntry{};
    expectedEntry.modulebase = moduleBase;
    expectedEntry.modulepart = modulePart;
    expectedEntry.modulesize = memoryRegionSize;
    //expectedEntry.modulefileoffset = fileOffset;
    expectedEntry.result = 1;
    expectedEntry.modulenamesize = moduleName.size();

    auto actualEntry = CeModuleEntry{};
    char actualModuleName[1024];

    //
    {
        InSequence seq;

        EXPECT_CALL(*socket, send(_, sizeof(CeModuleEntry), 0))
                .WillOnce(DoAll(Invoke([&actualEntry](const char *buffer, size_t size, int flags) {
                    memcpy(&actualEntry, buffer, size);
                    return size;
                }), Return(sizeof(CeModuleEntry))));
        EXPECT_CALL(*socket, send(_, moduleName.size(), 0))
                .WillOnce(Invoke([&actualModuleName](const char *buffer, size_t size, int flags) {
                    memcpy(actualModuleName, buffer, size);
                    actualModuleName[size] = 0;
                    return size;
                }));
    }

    client->sendModule32Entry(true, moduleBase, modulePart, memoryRegionSize, fileOffset, moduleName);
    EXPECT_STREQ(actualModuleName, moduleName.c_str());

    EXPECT_EQ(actualEntry.result, expectedEntry.result);
    EXPECT_EQ(actualEntry.modulebase, expectedEntry.modulebase);
    EXPECT_EQ(actualEntry.modulesize, expectedEntry.modulesize);
    //EXPECT_EQ(actualEntry.modulefileoffset, expectedEntry.modulefileoffset);
    EXPECT_EQ(actualEntry.modulepart, expectedEntry.modulepart);
    EXPECT_EQ(actualEntry.modulenamesize, expectedEntry.modulenamesize);
}

TEST_F(ClientTest, SendModule32EntryEndOfList) {
    auto expectedEntry = CeModuleEntry{};
    expectedEntry.modulebase = 0;
    expectedEntry.modulepart = 0;
    expectedEntry.modulesize = 0;
    //expectedEntry.modulefileoffset = 0;
    expectedEntry.result = 0;
    expectedEntry.modulenamesize = 0;

    auto actualEntry = CeModuleEntry{};

    EXPECT_CALL(*socket, send(_, sizeof(CeModuleEntry), 0))
            .WillOnce(DoAll(Invoke([&actualEntry](const char *buffer, size_t size, int flags) {
                memcpy(&actualEntry, buffer, size);
                return size;
            }), Return(sizeof(CeModuleEntry))));
    // nothing on the wire for empty module name

    client->sendModule32Entry(false, 0, 0, 0, 0, "");

    EXPECT_EQ(actualEntry.result, expectedEntry.result);
    EXPECT_EQ(actualEntry.modulebase, expectedEntry.modulebase);
    EXPECT_EQ(actualEntry.modulesize, expectedEntry.modulesize);
    //EXPECT_EQ(actualEntry.modulefileoffset, expectedEntry.modulefileoffset);
    EXPECT_EQ(actualEntry.modulepart, expectedEntry.modulepart);
    EXPECT_EQ(actualEntry.modulenamesize, expectedEntry.modulenamesize);
}

TEST_F(ClientTest, CreateProcessSnapshot) {
    Sequence recv, send;
    int flags = TH32CS_SNAPPROCESS;
    int procId = 0;
    std::vector<ProcessInfo> snapshot;
    // first handle should be 1 - zero is seen as an error by CE
    DWORD expectedHandle = 1;

    snapshot.push_back(ProcessInfo("explorer.exe", 123));
    snapshot.push_back(ProcessInfo("chrome.exe", 124)); {
        InSequence setup;

        /*******************************
         * CMD_CREATETOOLHELP32SNAPSHOT
         *******************************/

        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(flags), Return(sizeof(DWORD))))
                .RetiresOnSaturation();
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(procId), Return(sizeof(DWORD))))
                .RetiresOnSaturation();

        EXPECT_CALL(*memory, getModuleSnapshot(_)).Times(0)
                .RetiresOnSaturation();
        EXPECT_CALL(*memory, getProcessSnapshot()).WillOnce(Return(snapshot))
                .RetiresOnSaturation();

        EXPECT_CALL(*socket, send(Pointee(Eq(expectedHandle)), sizeof(DWORD), 0))
                .WillOnce(Return(sizeof(DWORD)))
                .RetiresOnSaturation();
    }

    EXPECT_TRUE(client->dispatchCommand(CMD_CREATETOOLHELP32SNAPSHOT));
    EXPECT_TRUE(client->state_->containsProcessInfoHandle(expectedHandle));
}

TEST_F(ClientTest, CreateModuleSnapshot) {
    Sequence recv, send;
    int flags = TH32CS_SNAPMODULE;
    int procId = 0;
    std::vector<MemoryRegion> snapshot;
    // first handle should be 1 - zero is seen as an error by CE
    DWORD expectedHandle = 1;

    VMMDLL_MAP_VADENTRY region1{};
    region1.uszText = (LPSTR) "region1";
    region1.vaStart = 0x123;
    region1.vaEnd = 0x133;
    snapshot.push_back(MemoryRegion(region1));
    VMMDLL_MAP_VADENTRY region2{};
    region2.uszText = (LPSTR) "region2";
    region1.vaStart = 0x124;
    region1.vaEnd = 0x135;
    snapshot.push_back(MemoryRegion(region2)); {
        InSequence setup;

        /*******************************
         * CMD_CREATETOOLHELP32SNAPSHOT
         *******************************/

        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(flags), Return(sizeof(DWORD))))
                .RetiresOnSaturation();
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(procId), Return(sizeof(DWORD))))
                .RetiresOnSaturation();

        EXPECT_CALL(*memory, getModuleSnapshot(_)).WillOnce(Return(snapshot))
                .RetiresOnSaturation();
        EXPECT_CALL(*memory, getProcessSnapshot()).Times(0)
                .RetiresOnSaturation();

        EXPECT_CALL(*socket, send(Pointee(Eq(expectedHandle)), sizeof(DWORD), 0))
                .WillOnce(Return(sizeof(DWORD)))
                .RetiresOnSaturation();
    }

    EXPECT_TRUE(client->dispatchCommand(CMD_CREATETOOLHELP32SNAPSHOT));
    EXPECT_TRUE(client->state_->containsMemoryRegionHandle(expectedHandle));
}

TEST_F(ClientTest, CreateThreadSnapshotEx) {
    VMMDLL_MAP_THREADENTRY thread1{};
    thread1.dwPID = 123;
    VMMDLL_MAP_THREADENTRY thread2{};
    thread2.dwPID = 124563;

    std::vector<ThreadInfo> snapshot;
    snapshot.push_back(ThreadInfo(thread1));
    snapshot.push_back(ThreadInfo(thread2));

    int processID = 111; {
        InSequence seq;

        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(TH32CS_SNAPTHREAD), Return(sizeof(DWORD))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(processID), Return(sizeof(DWORD))));

        EXPECT_CALL(*memory, getThreadSnapshot(processID)).WillOnce(Return(snapshot));
    }

    EXPECT_CALL(*socket, send(Pointee(Eq(snapshot.size())), sizeof(int), 0));
    EXPECT_CALL(*mockClient, sendThread32Entry(thread1.dwTID)).RetiresOnSaturation();
    EXPECT_CALL(*mockClient, sendThread32Entry(thread2.dwTID)).RetiresOnSaturation();

    EXPECT_TRUE(mockClient->dispatchCommand(CMD_CREATETOOLHELP32SNAPSHOTEX));
}

TEST_F(ClientTest, CreateProcessSnapshotEx) { {
        InSequence seq;

        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(TH32CS_SNAPPROCESS), Return(sizeof(DWORD))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(0), Return(sizeof(DWORD))));
    }

    // CE hasn't implemented the rest yet, so it falls back to normal process32next behaviour
    EXPECT_CALL(*memory, getProcessSnapshot());
    EXPECT_CALL(*socket, send(Pointee(Eq(1)), sizeof(int32_t), 0));

    EXPECT_TRUE(mockClient->dispatchCommand(CMD_CREATETOOLHELP32SNAPSHOTEX));
}

TEST_F(ClientTest, CreateModuleSnapshotEx) {
    Sequence recv, send;
    int procId = 123;
    std::vector<MemoryRegion> snapshot;

    VMMDLL_MAP_VADENTRY region1{};
    region1.uszText = (LPSTR) "region1";
    region1.vaStart = 0x123;
    region1.vaEnd = 0x124;
    snapshot.push_back(MemoryRegion(region1));
    VMMDLL_MAP_VADENTRY region2{};
    region2.uszText = (LPSTR) "region2";
    region1.vaStart = 0x125;
    region1.vaEnd = 0x131;
    snapshot.push_back(MemoryRegion(region2)); {
        InSequence setup;

        /*******************************
         * CMD_CREATETOOLHELP32SNAPSHOTEX
         *******************************/

        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(TH32CS_SNAPMODULE), Return(sizeof(DWORD))))
                .RetiresOnSaturation();
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(procId), Return(sizeof(DWORD))))
                .RetiresOnSaturation();

        EXPECT_CALL(*memory, getModuleSnapshot(procId)).WillOnce(Return(snapshot))
                .RetiresOnSaturation();
        EXPECT_CALL(*memory, getProcessSnapshot()).Times(0)
                .RetiresOnSaturation();
    }

    // SnapshotEx sends entries immediately
    {
        InSequence seq;

        EXPECT_CALL(*mockClient,
                    sendModule32Entry(true, snapshot[0].startAddr, snapshot[0].part, snapshot[0].regionSize,snapshot[0].
                        fileOffset, snapshot[0].name));
        EXPECT_CALL(*mockClient,
                    sendModule32Entry(true, snapshot[1].startAddr, snapshot[1].part, snapshot[1].regionSize,snapshot[1].
                        fileOffset, snapshot[1].name));
        EXPECT_CALL(*mockClient, sendModule32Entry(false, 0, 0, 0, 0, ""));
    }

    EXPECT_TRUE(mockClient->dispatchCommand(CMD_CREATETOOLHELP32SNAPSHOTEX));
    // no handle is stored or sent on the wire
}

TEST_F(ClientTest, Process32First) {
    std::vector<ProcessInfo> snapshot;
    DWORD expectedHandle = 123;

    snapshot.push_back(ProcessInfo("explorer.exe", 123));
    snapshot.push_back(ProcessInfo("chrome.exe", 124));

    mockClient->state_->storeTlHelp32Snapshot(snapshot, expectedHandle);
    mockClient->state_->processIter[expectedHandle] = 999999;

    EXPECT_CALL(*socket, recv(Pointee(_), sizeof(DWORD), MSG_WAITALL))
            .WillOnce(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(DWORD))));
    EXPECT_CALL(*mockClient, sendProcess32Entry(true, snapshot[0].pid, snapshot[0].name));
    EXPECT_TRUE(mockClient->dispatchCommand(CMD_PROCESS32FIRST));
}

TEST_F(ClientTest, Module32First) {
    std::vector<MemoryRegion> snapshot;
    DWORD expectedHandle = 123;

    VMMDLL_MAP_VADENTRY region1{};
    region1.uszText = (LPSTR) "region1";
    region1.vaStart = 0x123;
    region1.vaEnd = 0x124;
    snapshot.push_back(MemoryRegion(region1));
    VMMDLL_MAP_VADENTRY region2{};
    region2.uszText = (LPSTR) "region2";
    region1.vaStart = 0x125;
    region1.vaEnd = 0x131;
    snapshot.push_back(MemoryRegion(region2));

    mockClient->state_->storeTlHelp32Snapshot(snapshot, expectedHandle);
    mockClient->state_->moduleIter[expectedHandle] = 999999;

    EXPECT_CALL(*socket, recv(Pointee(_), sizeof(DWORD), MSG_WAITALL))
            .WillOnce(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(DWORD))));
    EXPECT_CALL(*mockClient,
                sendModule32Entry(true, snapshot[0].startAddr, snapshot[0].part, snapshot[0].regionSize, snapshot[0].
                    fileOffset, snapshot[0].name));
    EXPECT_TRUE(mockClient->dispatchCommand(CMD_MODULE32FIRST));
}

TEST_F(ClientTest, Process32Next) {
    std::vector<ProcessInfo> snapshot;
    DWORD expectedHandle = 123;

    snapshot.push_back(ProcessInfo("explorer.exe", 123));
    snapshot.push_back(ProcessInfo("chrome.exe", 124));

    mockClient->state_->storeTlHelp32Snapshot(snapshot, expectedHandle);

    EXPECT_CALL(*socket, recv(Pointee(_), sizeof(DWORD), MSG_WAITALL))
            .WillRepeatedly(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(DWORD)))); {
        InSequence seq;
        EXPECT_CALL(*mockClient, sendProcess32Entry(true, snapshot[0].pid, snapshot[0].name)).Times(1);
        EXPECT_CALL(*mockClient, sendProcess32Entry(true, snapshot[1].pid, snapshot[1].name)).Times(1);
        EXPECT_CALL(*mockClient, sendProcess32Entry(false, 0, "")).Times(1);
    }
    EXPECT_TRUE(mockClient->dispatchCommand(CMD_PROCESS32NEXT)); // proc 1
    EXPECT_TRUE(mockClient->dispatchCommand(CMD_PROCESS32NEXT)); // proc 2
    EXPECT_TRUE(mockClient->dispatchCommand(CMD_PROCESS32NEXT)); // end of list
}

TEST_F(ClientTest, Module32Next) {
    std::vector<MemoryRegion> snapshot;
    DWORD expectedHandle = 123;

    VMMDLL_MAP_VADENTRY region1{};
    region1.uszText = (LPSTR) "region1";
    region1.vaStart = 0x123;
    region1.vaEnd = 0x133;
    snapshot.push_back(MemoryRegion(region1));
    VMMDLL_MAP_VADENTRY region2{};
    region2.uszText = (LPSTR) "region2";
    region1.vaStart = 0x124;
    region1.vaEnd = 0x135;
    snapshot.push_back(MemoryRegion(region2));

    mockClient->state_->storeTlHelp32Snapshot(snapshot, expectedHandle);

    EXPECT_CALL(*socket, recv(Pointee(_), sizeof(DWORD), MSG_WAITALL))
            .WillRepeatedly(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(DWORD)))); {
        InSequence seq;

        EXPECT_CALL(*mockClient,
                    sendModule32Entry(true, snapshot[0].startAddr, snapshot[0].part, snapshot[0].regionSize, snapshot[0]
                        .fileOffset, snapshot[0].name));
        EXPECT_CALL(*mockClient,
                    sendModule32Entry(true, snapshot[1].startAddr, snapshot[1].part, snapshot[1].regionSize, snapshot[1]
                        .fileOffset, snapshot[1].name));
        EXPECT_CALL(*mockClient, sendModule32Entry(false, 0, 0, 0, 0, ""));
    }

    EXPECT_TRUE(mockClient->dispatchCommand(CMD_MODULE32NEXT)); // module 1
    EXPECT_TRUE(mockClient->dispatchCommand(CMD_MODULE32NEXT)); // module 2
    EXPECT_TRUE(mockClient->dispatchCommand(CMD_MODULE32NEXT)); // end of list
}

TEST_F(ClientTest, CloseHandle) {
    DWORD expectedHandle = 123;

    std::vector<ProcessInfo> processSnapshot;
    processSnapshot.push_back(ProcessInfo("explorer.exe", 123));

    std::vector<MemoryRegion> moduleSnapshot;
    VMMDLL_MAP_VADENTRY region1{};
    region1.uszText = (LPSTR) "region1";
    region1.vaStart = 0x123;
    region1.vaEnd = 0x130;
    moduleSnapshot.push_back(MemoryRegion(region1));

    mockClient->state_->storeTlHelp32Snapshot(processSnapshot, expectedHandle);
    mockClient->state_->storeTlHelp32Snapshot(moduleSnapshot, expectedHandle);
    mockClient->state_->storeOpenProcess(123, expectedHandle);

    EXPECT_CALL(*socket, send(Pointee(Eq(1)), sizeof(int), 0));
    EXPECT_CALL(*socket, recv(Pointee(_), sizeof(DWORD), MSG_WAITALL))
            .WillOnce(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(DWORD))));

    EXPECT_TRUE(mockClient->dispatchCommand(CMD_CLOSEHANDLE));
    EXPECT_FALSE(mockClient->state_->containsProcessInfoHandle(expectedHandle));
    EXPECT_FALSE(mockClient->state_->containsMemoryRegionHandle(expectedHandle));
    EXPECT_FALSE(mockClient->state_->containsProcessIDHandle(expectedHandle));
}

TEST_F(ClientTest, OpenProcess) {
    int processID = 123;
    int expectedHandle = 1;

    EXPECT_CALL(*socket, recv(Pointee(_), sizeof(int), MSG_WAITALL))
            .WillOnce(DoAll(SetArgPointee<0>(processID), Return(sizeof(int))));
    EXPECT_CALL(*socket, send(Pointee(Eq(expectedHandle)), sizeof(int), 0));

    EXPECT_TRUE(client->dispatchCommand(CMD_OPENPROCESS));
    EXPECT_EQ(client->state_->getProcessID(expectedHandle), processID);
}

TEST_F(ClientTest, ReadProcessMemoryWithCompression) {
    CE_HANDLE expectedHandle = 123;
    uint64_t address = 123;
    size_t size = 10;
    bool compress = true; {
        InSequence seq;
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(CE_HANDLE), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(CE_HANDLE))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint64_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(address), Return(sizeof(uint64_t))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(int32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(size), Return(sizeof(int32_t))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(char), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(compress), Return(sizeof(char))));
    }

    // not supported yet
    EXPECT_FALSE(client->dispatchCommand(CMD_READPROCESSMEMORY));
}

TEST_F(ClientTest, ReadProcessMemoryNoProcessOpened) {
    CE_HANDLE expectedHandle = 123;
    uint64_t address = 123;
    size_t size = 10;
    bool compress = false; {
        InSequence seq;
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(CE_HANDLE), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(CE_HANDLE))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint64_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(address), Return(sizeof(uint64_t))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(int32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(size), Return(sizeof(int32_t))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(char), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(compress), Return(sizeof(char))));
    }

    EXPECT_FALSE(client->dispatchCommand(CMD_READPROCESSMEMORY));
}

TEST_F(ClientTest, ReadProcessMemory) {
    CE_HANDLE expectedHandle = 123;
    int32_t processID = 1;
    uint64_t address = 123;
    bool compress = false;
    const char *expectedContents = "asdfasdfasdf";
    size_t size = strlen(expectedContents); {
        InSequence seq;
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(CE_HANDLE), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(CE_HANDLE))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint64_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(address), Return(sizeof(uint64_t))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(int32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(size), Return(sizeof(int32_t))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(char), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(compress), Return(sizeof(char))));
    }

    client->state_->storeOpenProcess(processID, expectedHandle);

    EXPECT_CALL(*memory, read(Pointee(_), processID, address, size))
            .WillOnce(DoAll(
                Invoke([&expectedContents](char *buffer, int32_t processID, uint64_t address, size_t size) {
                    memcpy(buffer, expectedContents, size);
                }), Return(size)));

    char actualContents[1024];
    EXPECT_CALL(*socket, send(Pointee(Eq(size)), sizeof(int32_t), 0));
    EXPECT_CALL(*socket, send(Pointee(_), size, 0))
            .WillOnce(DoAll(Invoke([&actualContents](const char *buffer, size_t size, int flags) {
                memcpy(actualContents, buffer, size);
                actualContents[size] = 0;
            }), Return(size)));

    EXPECT_TRUE(client->dispatchCommand(CMD_READPROCESSMEMORY));
    EXPECT_STREQ(actualContents, expectedContents);
}

TEST_F(ClientTest, WriteProcessMemoryNoProcessOpened) {
    CE_HANDLE expectedHandle = 123;
    uint64_t address = 123;
    const char *contents = "aasdfasdf";
    size_t size = strlen(contents); {
        InSequence seq;
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(CE_HANDLE), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(CE_HANDLE))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint64_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(address), Return(sizeof(uint64_t))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(int32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(size), Return(sizeof(int32_t))));
        EXPECT_CALL(*socket, recv(Pointee(_), size, MSG_WAITALL))
                .WillOnce(DoAll(Invoke([&contents](char *buffer, size_t size, int flags) {
                    return memcpy(buffer, contents, size);
                }), Return(size)));
    }

    EXPECT_FALSE(client->dispatchCommand(CMD_WRITEPROCESSMEMORY));
}

TEST_F(ClientTest, WriteProcessMemory) {
    CE_HANDLE expectedHandle = 123;
    uint64_t address = 123;
    int32_t processID = 4;
    const char *contents = "aasdfasdf";
    size_t size = strlen(contents);
    char actualContents[1024]; {
        InSequence seq;
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(CE_HANDLE), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(CE_HANDLE))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint64_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(address), Return(sizeof(uint64_t))));
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(int32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(size), Return(sizeof(int32_t))));
        EXPECT_CALL(*socket, recv(Pointee(_), size, MSG_WAITALL))
                .WillOnce(DoAll(Invoke([&contents](char *buffer, size_t size, int flags) {
                    return memcpy(buffer, contents, size);
                }), Return(size)));
    }

    EXPECT_CALL(*socket, send(Pointee(Eq(size)), sizeof(int32_t), 0));

    EXPECT_CALL(*memory, write(Pointee(_), processID, address, size))
            .WillOnce(DoAll(
                Invoke([&actualContents](const char *buffer, int32_t processID, uint64_t address, size_t size) {
                    memcpy(actualContents, buffer, size);
                    actualContents[size] = 0;
                }), Return(size)));

    client->state_->storeOpenProcess(processID, expectedHandle);

    EXPECT_TRUE(client->dispatchCommand(CMD_WRITEPROCESSMEMORY));
    EXPECT_STREQ(actualContents, contents);
}

TEST_F(ClientTest, SystemArchitecture) {
    BYTE expectedArch = 1;
    CE_HANDLE expectedHandle = 123;
    DWORD processID = 456;

    mockClient->state_->storeOpenProcess(processID, expectedHandle);

    EXPECT_CALL(*socket, recvNonBlocking(_, sizeof(CE_HANDLE), 0))
            .WillOnce(DoAll(Invoke([&expectedHandle](char* buffer, size_t size, int flags) {
                memcpy(buffer, &expectedHandle, size);
                return size;
            }), Return(sizeof(CE_HANDLE))));
    EXPECT_CALL(*socket, lastError()).WillOnce(Return(0));
    EXPECT_CALL(*memory, getMemoryModel(processID)).WillOnce(Return(MemoryModel::MemoryModel_X64));
    EXPECT_CALL(*mockClient, getArchitectureFromMemoryModel(_))
            .WillOnce(Return(expectedArch));
    EXPECT_CALL(*socket, send(Pointee(Eq(expectedArch)), sizeof(unsigned char), 0));
    EXPECT_TRUE(mockClient->dispatchCommand(CMD_GETARCHITECTURE));
}


TEST_F(ClientTest, GetArchitectureFromMemoryModel) {
    EXPECT_EQ(0xFF, client->getArchitectureFromMemoryModel(MemoryModel::MemoryModel_UNKNOWN));
    EXPECT_EQ(0xFF, client->getArchitectureFromMemoryModel(MemoryModel::MemoryModel_NA));

    EXPECT_EQ(0, client->getArchitectureFromMemoryModel(MemoryModel::MemoryModel_X86));
    EXPECT_EQ(0, client->getArchitectureFromMemoryModel(MemoryModel::MemoryModel_X86PAE));

    EXPECT_EQ(1, client->getArchitectureFromMemoryModel(MemoryModel::MemoryModel_X64));

    EXPECT_EQ(3, client->getArchitectureFromMemoryModel(MemoryModel::MemoryModel_ARM64));
}

TEST_F(ClientTest, GetSymbolListFromFile) {
    //int32_t fileOffset = 0;
    const char *symbolPath = "asdfasdf";
    int32_t symbolPathSize = strlen(symbolPath);

    // not supported, so return 0 all the time
    EXPECT_CALL(*socket, send(Pointee(Eq(0)), sizeof(uint64_t), 0)); {
        InSequence seq;
        // offset
        // EXPECT_CALL(*socket, recv(Pointee(_), sizeof(int32_t), MSG_WAITALL))
        //         .WillOnce(DoAll(SetArgPointee<0>(fileOffset), Return(sizeof(int32_t))));
        // path size
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(int32_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(symbolPathSize), Return(sizeof(int32_t))));
        // path itself
        EXPECT_CALL(*socket, recv(Pointee(_), symbolPathSize, MSG_WAITALL))
                .WillOnce(DoAll(Invoke([&symbolPath](char *buffer, size_t size, int flags) {
                    return memcpy(buffer, symbolPath, size);
                }), Return(symbolPathSize)));
    }
    EXPECT_TRUE(client->dispatchCommand(CMD_GETSYMBOLLISTFROMFILE));
}

TEST_F(ClientTest, SendVirtualQueryExResult) {
    VMMDLL_MAP_VADENTRY vadEntry{};
    vadEntry.uszText = (LPSTR) "region1";
    vadEntry.vaStart = 0;
    vadEntry.vaEnd = 10;
    vadEntry.Protection = 1; // PAGE_READONLY
    vadEntry.VadType = 1; // MEM_MAPPED

    int32_t expectedProtection = PAGE_READONLY;
    int32_t expectedType = MEM_MAPPED;

    MemoryRegion region(vadEntry);

    auto expectedEntry = CeVirtualQueryExOutput{};
    expectedEntry.result = 1;
    expectedEntry.protection = expectedProtection;
    expectedEntry.type = expectedType;
    expectedEntry.baseaddress = vadEntry.vaStart;
    expectedEntry.size = vadEntry.vaEnd - vadEntry.vaStart + 1;

    auto actualEntry = CeVirtualQueryExOutput{};

    EXPECT_CALL(*socket, send(_, sizeof(CeVirtualQueryExOutput), 0))
            .WillOnce(DoAll(Invoke([&actualEntry](const char *buffer, size_t size, int flags) {
                memcpy(&actualEntry, buffer, size);
            }), Return(sizeof(CeVirtualQueryExOutput))));

    client->sendVirtualQueryExResult(&region);

    EXPECT_EQ(actualEntry.result, expectedEntry.result);
    EXPECT_EQ(actualEntry.protection, expectedEntry.protection);
    EXPECT_EQ(actualEntry.type, expectedEntry.type);
    EXPECT_EQ(actualEntry.baseaddress, expectedEntry.baseaddress);
    EXPECT_EQ(actualEntry.size, expectedEntry.size);
}

TEST_F(ClientTest, SendVirtualQueryExResultNull) {
    auto expectedEntry = CeVirtualQueryExOutput{};
    expectedEntry.result = 0;
    expectedEntry.protection = 0;
    expectedEntry.type = 0;
    expectedEntry.baseaddress = 0;
    expectedEntry.size = 0;

    auto actualEntry = CeVirtualQueryExOutput{};

    EXPECT_CALL(*socket, send(_, sizeof(CeVirtualQueryExOutput), 0))
            .WillOnce(DoAll(Invoke([&actualEntry](const char *buffer, size_t size, int flags) {
                memcpy(&actualEntry, buffer, size);
            }), Return(sizeof(CeVirtualQueryExOutput))));

    client->sendVirtualQueryExResult(nullptr);

    EXPECT_EQ(actualEntry.result, expectedEntry.result);
    EXPECT_EQ(actualEntry.protection, expectedEntry.protection);
    EXPECT_EQ(actualEntry.type, expectedEntry.type);
    EXPECT_EQ(actualEntry.baseaddress, expectedEntry.baseaddress);
    EXPECT_EQ(actualEntry.size, expectedEntry.size);
}

TEST_F(ClientTest, SendVirtualQueryExFullResult) {
    VMMDLL_MAP_VADENTRY vadEntry{};
    vadEntry.uszText = (LPSTR) "region1";
    vadEntry.vaStart = 0;
    vadEntry.vaEnd = 10;
    vadEntry.Protection = 1; // PAGE_READONLY
    vadEntry.VadType = 1; // MEM_MAPPED

    int32_t expectedProtection = PAGE_READONLY;
    int32_t expectedType = MEM_MAPPED;

    MemoryRegion region(vadEntry);

    auto expectedEntry = CeRegionInfo{};
    expectedEntry.protection = expectedProtection;
    expectedEntry.type = expectedType;
    expectedEntry.baseaddress = vadEntry.vaStart;
    expectedEntry.size = vadEntry.vaEnd - vadEntry.vaStart + 1;

    auto actualEntry = CeRegionInfo{};

    EXPECT_CALL(*socket, send(_, sizeof(CeRegionInfo), 0))
            .WillOnce(DoAll(Invoke([&actualEntry](const char *buffer, size_t size, int flags) {
                memcpy(&actualEntry, buffer, size);
            }), Return(sizeof(CeRegionInfo))));

    client->sendVirtualQueryExFullResult(&region);

    EXPECT_EQ(actualEntry.protection, expectedEntry.protection);
    EXPECT_EQ(actualEntry.type, expectedEntry.type);
    EXPECT_EQ(actualEntry.baseaddress, expectedEntry.baseaddress);
    EXPECT_EQ(actualEntry.size, expectedEntry.size);
}

TEST_F(ClientTest, GetRegionInfo) {
    std::vector<MemoryRegion> snapshot;
    CE_HANDLE expectedHandle = 5;
    uint64_t address = 15;
    int32_t processID = 2;
    unsigned char expectedMaxLength = 127;

    VMMDLL_MAP_VADENTRY vadEntry1{};
    vadEntry1.uszText = (LPSTR) "region1";
    vadEntry1.vaStart = 0;
    vadEntry1.vaEnd = 10;
    vadEntry1.Protection = 1; // PAGE_READONLY
    vadEntry1.VadType = 1; // MEM_MAPPED
    snapshot.push_back(MemoryRegion(vadEntry1));
    VMMDLL_MAP_VADENTRY vadEntry2{};
    // make it over 127 chars
    vadEntry2.uszText = (LPSTR)
            "region2xxxregion2xxxregion2xxxregion2xxxregion2xxxregion2xxxregion2xxxregion2xxxregion2xxxregion2xxxregion2xxxregion2xxxregion2xxx";
    vadEntry2.vaStart = 11;
    vadEntry2.vaEnd = 20;
    vadEntry2.Protection = 6; // PAGE_EXECUTE_READWRITE
    vadEntry2.VadType = 2; // MEM_IMAGE
    snapshot.push_back(MemoryRegion(vadEntry2));
    VMMDLL_MAP_VADENTRY vadEntry3{};
    vadEntry3.uszText = (LPSTR) "region3";
    vadEntry3.vaStart = 21;
    vadEntry3.vaEnd = 45;
    vadEntry3.Protection = 6; // PAGE_EXECUTE_READWRITE
    vadEntry3.VadType = 2; // MEM_IMAGE
    snapshot.push_back(MemoryRegion(vadEntry3));

    //
    {
        InSequence seq;
        // offset
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(CE_HANDLE), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(CE_HANDLE))));
        // address
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(uint64_t), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(address), Return(sizeof(uint64_t))));
    }

    mockClient->state_->storeOpenProcess(processID, expectedHandle);
    EXPECT_CALL(*memory, getModuleSnapshot(processID))
            .WillOnce(Return(snapshot));

    char actualSentName[256] = {0}; // anything greater than 127 (expectedMaxLength) will suffice
    {
        InSequence seq;

        // virtualqueryex payload
        EXPECT_CALL(*mockClient, sendVirtualQueryExResult);

        // memory region payload
        EXPECT_CALL(*socket, send(Pointee(expectedMaxLength), sizeof(BYTE), 0))
                .WillOnce(Return(sizeof(BYTE)));
        EXPECT_CALL(*socket, send(_, 127, 0))
                .WillOnce(DoAll(Invoke([&actualSentName](const char *buffer, size_t size, int flags) {
                    memcpy(actualSentName, buffer, size);
                    actualSentName[size] = 0;
                }), Return(127)));
    }

    EXPECT_TRUE(mockClient->dispatchCommand(CMD_GETREGIONINFO));
    EXPECT_STREQ(actualSentName, snapshot[1].name.substr(0, 127).c_str());
    EXPECT_EQ(strlen(actualSentName), expectedMaxLength);
}

TEST_F(ClientTest, VirtualQueryExFull) {
    std::vector<MemoryRegion> snapshot;
    CE_HANDLE expectedHandle = 5;
    uint64_t address = 15;
    int32_t processID = 2;
    unsigned char expectedMaxLength = 127;

    VMMDLL_MAP_VADENTRY vadEntry1{};
    vadEntry1.Protection = 1; // PAGE_READONLY
    vadEntry1.VadType = 1; // MEM_MAPPED
    vadEntry1.uszText = (LPSTR)"region1";
    vadEntry1.vaStart = 0;
    vadEntry1.vaEnd = 10;
    snapshot.push_back(MemoryRegion(vadEntry1));
    VMMDLL_MAP_VADENTRY vadEntry2{};
    vadEntry2.Protection = 6; // PAGE_EXECUTE_READWRITE
    vadEntry2.VadType = 2; // MEM_IMAGE
    vadEntry1.uszText = (LPSTR)"region2";
    vadEntry1.vaStart = 11;
    vadEntry1.vaEnd = 20;
    snapshot.push_back(MemoryRegion(vadEntry2));
    VMMDLL_MAP_VADENTRY vadEntry3{};
    vadEntry3.Protection = 6; // PAGE_EXECUTE_READWRITE
    vadEntry3.VadType = 2; // MEM_IMAGE
    vadEntry1.uszText = (LPSTR)"region2";
    vadEntry1.vaStart = 21;
    vadEntry1.vaEnd = 50;
    snapshot.push_back(MemoryRegion(vadEntry3));

    mockClient->state_->storeOpenProcess(processID, expectedHandle); {
        InSequence seq;

        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(CE_HANDLE), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(expectedHandle), Return(sizeof(CE_HANDLE))));
        // flags (unused)
        EXPECT_CALL(*socket, recv(Pointee(_), sizeof(BYTE), MSG_WAITALL))
                .WillOnce(DoAll(SetArgPointee<0>(0), Return(sizeof(BYTE))));
    }

    EXPECT_CALL(*memory, getModuleSnapshot(processID))
            .WillOnce(Return(snapshot)); {
        InSequence seq;

        EXPECT_CALL(*socket, send(Pointee(snapshot.size()), sizeof(int32_t), 0))
                .WillOnce(Return(sizeof(int32_t)));
        EXPECT_CALL(*mockClient, sendVirtualQueryExFullResult).Times(3);
    }

    EXPECT_TRUE(mockClient->dispatchCommand(CMD_VIRTUALQUERYEXFULL));
}

TEST_F(ClientTest, StartDebug) {
    // currently not implemented
    auto expectedHandle = 123;
    EXPECT_CALL(*socket, recv(_, sizeof(CE_HANDLE), MSG_WAITALL))
        .WillOnce(DoAll(Invoke([&expectedHandle](char* buffer, size_t size, int flags) {
            memset(buffer, expectedHandle, size);
            return size;
        }), Return(sizeof(CE_HANDLE))));
    EXPECT_CALL(*socket, send(Pointee(Eq(expectedHandle)), sizeof(int), 0));

    EXPECT_TRUE(client->dispatchCommand(CMD_STARTDEBUG));
}

TEST_F(ClientTest, GetOptions) {
    // currently not implemented so expect to return a zero length
    EXPECT_CALL(*socket, send(Pointee(Eq(0)), sizeof(uint16_t), 0));

    EXPECT_TRUE(client->dispatchCommand(CMD_GETOPTIONS));
}
