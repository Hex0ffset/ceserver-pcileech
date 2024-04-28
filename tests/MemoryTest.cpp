#include "../Memory.h"
#include "mocks/Memory.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <absl/log/log.h>

#include "mocks/PCILeechWrapper.h"

using ::testing::Return;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Eq;
using ::testing::DoAll;
using ::testing::_;
using ::testing::SetArgPointee;
using ::testing::InSequence;

struct tdVMM_HANDLE {
};

class MemoryTest : public ::testing::Test {
public:
    std::shared_ptr<Memory> memory = nullptr;
    std::shared_ptr<MockMemory> mockMemory = nullptr;
    std::shared_ptr<MockPCILeechWrapper> pcileech = nullptr;

    void SetUp() override {
        pcileech = std::make_shared<MockPCILeechWrapper>();
        mockMemory = std::make_shared<MockMemory>(pcileech);
        memory = std::make_shared<Memory>(mockMemory->mmapPath(), pcileech);
    }

    void TearDown() override {
    }
};

TEST_F(MemoryTest, Init) {
    EXPECT_CALL(*mockMemory, generateMemoryMap).WillOnce(Return(true));

    std::vector<std::string> actualArgv, expectedArgv;

    expectedArgv.push_back("-printf");
    expectedArgv.push_back("-v");
    expectedArgv.push_back("-device");
    expectedArgv.push_back("fpga");
    expectedArgv.push_back("-memmap");
    expectedArgv.push_back(memory->mmapPath());

    auto expectedHandle = (VMM_HANDLE) 1;

    EXPECT_CALL(*pcileech, VMMDLL_Initialize(_, _))
            .WillOnce(DoAll(Invoke([&actualArgv](DWORD argc, LPCSTR argv[]) {
                for (auto i = 0; i < argc; i++) {
                    auto s = std::string(argv[i]);
                    actualArgv.push_back(s);
                }
            }), Return(expectedHandle)));

    EXPECT_CALL(*pcileech, VMMDLL_Close(expectedHandle)); // dtor

    EXPECT_TRUE(mockMemory->init());

    EXPECT_EQ(actualArgv, expectedArgv);
}

TEST_F(MemoryTest, WriteMemory) {
    auto processID = 123;
    auto address = 10;
    const char *buffer = "foobar contents";
    auto size = strlen(buffer);
    char actualBuffer[1024];

    EXPECT_CALL(*pcileech, VMMDLL_MemWrite(_, processID, address, _, size))
            .WillOnce(DoAll(Invoke([&actualBuffer](VMM_HANDLE hVMM, DWORD dwPID, ULONG64 qwA, PBYTE pb, DWORD cb) {
                memcpy(actualBuffer, pb, cb);
                actualBuffer[cb] = 0;
            }), Return(size)));

    EXPECT_EQ(size, memory->write(buffer, processID, address, size));

    EXPECT_STREQ(actualBuffer, buffer);
}

TEST_F(MemoryTest, ReadMemoryScatter) {
    auto processID = 123;
    auto address = 10;
    char actualBuffer1[1024];
    char actualBuffer2[2048];
    char deduplicatedBuffer[2048];
    memset(actualBuffer1, 0, sizeof(actualBuffer1));
    memset(actualBuffer2, 0, sizeof(actualBuffer2));
    memset(deduplicatedBuffer, 0, sizeof(deduplicatedBuffer));
    auto expectedFlags = VMMDLL_FLAG_NOCACHE
                         | VMMDLL_FLAG_NOCACHEPUT
                         | VMMDLL_FLAG_ZEROPAD_ON_FAIL;

    auto expectedScatterHandle = (VMMDLL_SCATTER_HANDLE) 1; {
        InSequence seq;

        EXPECT_CALL(*pcileech, VMMDLL_Scatter_Initialize(_, processID, expectedFlags))
                .WillOnce(Return(expectedScatterHandle));
        EXPECT_CALL(*pcileech, VMMDLL_Scatter_Execute(expectedScatterHandle));
        EXPECT_CALL(*pcileech, VMMDLL_Scatter_Read(expectedScatterHandle, address, sizeof(actualBuffer1), _, _))
                .WillOnce(DoAll(
                    Invoke([&actualBuffer1](VMMDLL_SCATTER_HANDLE hS, QWORD va, DWORD cb, PBYTE pb, PDWORD pcbRead) {
                        char contents[] = "foo contents";
                        *pcbRead = strlen(contents);
                        memcpy(pb, contents, strlen(contents));
                        return true;
                    }), Return(true)));
        EXPECT_CALL(*pcileech, VMMDLL_Scatter_Read(expectedScatterHandle, address, sizeof(actualBuffer2), _, _))
                .WillOnce(DoAll(
                    Invoke([&actualBuffer1](VMMDLL_SCATTER_HANDLE hS, QWORD va, DWORD cb, PBYTE pb, PDWORD pcbRead) {
                        char contents[] = "bar";
                        *pcbRead = strlen(contents);
                        memcpy(pb, contents, strlen(contents));
                        return true;
                    }), Return(true)));
        EXPECT_CALL(*pcileech, VMMDLL_Scatter_CloseHandle(expectedScatterHandle));
    }

    std::thread scatterRead1([this, &processID, &address, &actualBuffer1, &actualBuffer2, &deduplicatedBuffer]() {
        EXPECT_EQ(memory->read(actualBuffer1, processID, address, sizeof(actualBuffer1)), strlen("foo contents"));
    });
    std::thread scatterRead2([this, &processID, &address, &actualBuffer1, &actualBuffer2, &deduplicatedBuffer]() {
        EXPECT_EQ(memory->read(actualBuffer2, processID, address, sizeof(actualBuffer2)), strlen("bar"));
    });
    std::thread scatterRead3([this, &processID, &address, &actualBuffer1, &actualBuffer2, &deduplicatedBuffer]() {
        // this should get de-duplicated because of the call with actualBuffer2 being the same pid, addr, and size
        EXPECT_EQ(memory->read(deduplicatedBuffer, processID, address, sizeof(deduplicatedBuffer)), strlen("bar"));
    });

    std::this_thread::sleep_for(std::chrono::seconds(1)); // give the 3 reads a chance to queue up
    LOG(INFO) << "Reads queued up. Starting dispatch thread";
    memory->start(); // start bg dispatch thread AFTER queuing the reads to ensure they batch up
    scatterRead1.join();
    scatterRead2.join();
    scatterRead3.join();

    EXPECT_STREQ(actualBuffer1, "foo contents");
    EXPECT_STREQ(actualBuffer2, "bar");
    EXPECT_STREQ(deduplicatedBuffer, "bar");
}

TEST_F(MemoryTest, ReadMemorySequential) {
    memory->start(); // start bg dispatch thread

    auto processID = 123;
    auto address = 10;
    char actualBuffer[1024];
    memset(actualBuffer, 0, sizeof(actualBuffer));
    auto expectedFlags = VMMDLL_FLAG_NOCACHE
                         | VMMDLL_FLAG_NOCACHEPUT
                         | VMMDLL_FLAG_ZEROPAD_ON_FAIL;

    EXPECT_CALL(*pcileech, VMMDLL_MemReadEx(_, processID, address, _, sizeof(actualBuffer), _, expectedFlags))
            .WillOnce(DoAll(Invoke([](VMM_HANDLE hVMM, DWORD dwPID, ULONG64 qwA, PBYTE pb,
                                      DWORD cb, PDWORD pcbReadOpt, ULONG64 flags) {
                char foobar[] = "foobar contents";
                *pcbReadOpt = strlen(foobar);
                memcpy(pb, foobar, *pcbReadOpt);

                return true;
            }), Return(true)));

    EXPECT_CALL(*pcileech, VMMDLL_MemFree(_));

    EXPECT_EQ(memory->read(actualBuffer, processID, address, sizeof(actualBuffer)), strlen("foobar contents"));

    EXPECT_STREQ(actualBuffer, "foobar contents");
}

TEST_F(MemoryTest, BacklogItem) {
    BacklogItem item(123, 0x100, 456);

    EXPECT_EQ(item.pid, 123);
    EXPECT_EQ(item.address, 0x100);
    EXPECT_EQ(item.size, 456);

    item.promise.set_value({strlen("foobar"), "foobar"});
    auto result = item.get();

    EXPECT_EQ(result.first, strlen("foobar"));
    EXPECT_STREQ(result.second, "foobar");

    for (int i = 0; i < 3; i++) {
        auto subsequentResult = item.get(); // should not throw for multiple fetches of the future
        EXPECT_EQ(subsequentResult.first, strlen("foobar"));
        EXPECT_STREQ(subsequentResult.second, "foobar");
    }
}

TEST_F(MemoryTest, ModuleSnapshot) {
    auto processID = 123;

    VMMDLL_MAP_VADENTRY region1{};
    region1.uszText = (LPSTR) "region1";
    region1.vaStart = 0x123;
    region1.vaEnd = 0x223;

    VMMDLL_MAP_VADENTRY region2{};
    region2.uszText = (LPSTR) "region2";
    region2.vaStart = 0x223;
    region2.vaEnd = 0x323;

    VMMDLL_MAP_VADENTRY region3{};
    region3.uszText = (LPSTR) "region3";
    region3.vaStart = 0x324;
    region3.vaEnd = 0x443;

    const int numRegions = 3;

    struct VMMDLL_MAP_VAD_impl {
        DWORD dwVersion;
        DWORD _Reserved1[4];
        DWORD cPage;
        PBYTE pbMultiText;
        DWORD cbMultiText;
        DWORD cMap;
        VMMDLL_MAP_VADENTRY pMap[numRegions];
    };

    PVMMDLL_MAP_VAD expectedVadMap = (VMMDLL_MAP_VAD *) new VMMDLL_MAP_VAD_impl();
    expectedVadMap->cMap = numRegions;
    expectedVadMap->pMap[0] = region1;
    expectedVadMap->pMap[1] = region2;
    expectedVadMap->pMap[2] = region3;

    std::vector<MemoryRegion> expectedSnapshot;
    expectedSnapshot.push_back(MemoryRegion(region1));
    expectedSnapshot.push_back(MemoryRegion(region2));
    expectedSnapshot.push_back(MemoryRegion(region3));

    EXPECT_CALL(*pcileech, VMMDLL_Map_GetVadU(_, processID, true, _))
            .WillOnce(DoAll(Invoke([&expectedVadMap](VMM_HANDLE hVMM, DWORD dwPID, BOOL fIdentifyModules,
                                                     PVMMDLL_MAP_VAD *ppVadMap) {
                *ppVadMap = expectedVadMap;
            }), Return(true)));
    EXPECT_CALL(*pcileech, VMMDLL_MemFree);

    auto actualSnapshot = memory->getModuleSnapshot(processID);

    EXPECT_EQ(actualSnapshot.size(), 3);
    EXPECT_STREQ(actualSnapshot[0].name.c_str(), expectedSnapshot[0].name.c_str());
    EXPECT_STREQ(actualSnapshot[1].name.c_str(), expectedSnapshot[1].name.c_str());
    EXPECT_STREQ(actualSnapshot[2].name.c_str(), expectedSnapshot[2].name.c_str());
}

TEST_F(MemoryTest, ProcessSnapshot) {
    std::vector<ProcessInfo> expectedProcInfos;
    expectedProcInfos.push_back(ProcessInfo("proc 1", 11));
    expectedProcInfos.push_back(ProcessInfo("proc 2", 25));
    expectedProcInfos.push_back(ProcessInfo("proc 3", 33));

    // first call gets num PIDs
    EXPECT_CALL(*pcileech, VMMDLL_PidList(_, NULL, Pointee(Eq(0))))
            .WillOnce(DoAll(Invoke([](VMM_HANDLE hVMM, PDWORD pPIDs, PSIZE_T pcPIDs) {
                *pcPIDs = 3;
            }), Return(true)));
    // 2nd call gets actual PIDs
    EXPECT_CALL(*pcileech, VMMDLL_PidList(_, _, Pointee(Eq(3))))
            .WillOnce(DoAll(Invoke([&expectedProcInfos](VMM_HANDLE hVMM, PDWORD pPIDs, PSIZE_T pcPIDs) {
                pPIDs[0] = expectedProcInfos[0].pid;
                pPIDs[1] = expectedProcInfos[1].pid;
                pPIDs[2] = expectedProcInfos[2].pid;
            }), Return(true)));

    EXPECT_CALL(
        *pcileech,
        VMMDLL_ProcessGetInformationString(_, expectedProcInfos[0].pid,
            VMMDLL_PROCESS_INFORMATION_OPT_STRING_PATH_USER_IMAGE))
            .WillOnce(Return((LPSTR) expectedProcInfos[0].name.c_str()));
    EXPECT_CALL(
        *pcileech,
        VMMDLL_ProcessGetInformationString(_, expectedProcInfos[1].pid,
            VMMDLL_PROCESS_INFORMATION_OPT_STRING_PATH_USER_IMAGE))
            .WillOnce(Return((LPSTR) expectedProcInfos[1].name.c_str()));
    EXPECT_CALL(
        *pcileech,
        VMMDLL_ProcessGetInformationString(_, expectedProcInfos[2].pid,
            VMMDLL_PROCESS_INFORMATION_OPT_STRING_PATH_USER_IMAGE))
            .WillOnce(Return((LPSTR) expectedProcInfos[2].name.c_str()));
    EXPECT_CALL(*pcileech, VMMDLL_MemFree);

    auto actualProcInfos = memory->getProcessSnapshot();

    ASSERT_EQ(actualProcInfos.size(), 3);
    ASSERT_STREQ(actualProcInfos[0].name.c_str(), expectedProcInfos[0].name.c_str());
    ASSERT_EQ(actualProcInfos[0].pid, expectedProcInfos[0].pid);
    ASSERT_STREQ(actualProcInfos[1].name.c_str(), expectedProcInfos[1].name.c_str());
    ASSERT_EQ(actualProcInfos[1].pid, expectedProcInfos[1].pid);
    ASSERT_STREQ(actualProcInfos[2].name.c_str(), expectedProcInfos[2].name.c_str());
    ASSERT_EQ(actualProcInfos[2].pid, expectedProcInfos[2].pid);
}

TEST_F(MemoryTest, ThreadSnapshot) {
    VMMDLL_MAP_THREADENTRY thread1{};
    thread1.dwPID = 123;
    VMMDLL_MAP_THREADENTRY thread2{};
    thread2.dwPID = 124563;

    const int numThreads = 2;
    struct VMMDLL_MAP_THREAD_impl {
        DWORD dwVersion;
        DWORD _Reserved[8];
        DWORD cMap;
        VMMDLL_MAP_THREADENTRY pMap[numThreads];
    };

    PVMMDLL_MAP_THREAD threadMap = (VMMDLL_MAP_THREAD *) new VMMDLL_MAP_THREAD_impl();
    threadMap->cMap = numThreads;
    threadMap->pMap[0] = thread1;
    threadMap->pMap[1] = thread2;

    std::vector<ThreadInfo> expectedThreadInfos;
    expectedThreadInfos.push_back(ThreadInfo(thread1));
    expectedThreadInfos.push_back(ThreadInfo(thread2));

    auto processID = 123;

    EXPECT_CALL(*pcileech, VMMDLL_Map_GetThread(_, processID, _))
            .WillOnce(DoAll(Invoke([&threadMap](VMM_HANDLE hVMM, DWORD dwPID, PVMMDLL_MAP_THREAD *ppThreadMap) {
                *ppThreadMap = threadMap;
            }), Return(true)));
    EXPECT_CALL(*pcileech, VMMDLL_MemFree(_));

    auto actualThreadInfos = memory->getThreadSnapshot(processID);

    EXPECT_EQ(actualThreadInfos.size(), numThreads);
    EXPECT_EQ(actualThreadInfos[0].threadentry.dwTID, expectedThreadInfos[0].threadentry.dwTID);
    EXPECT_EQ(actualThreadInfos[1].threadentry.dwTID, expectedThreadInfos[1].threadentry.dwTID);
}


TEST_F(MemoryTest, GenerateMemoryMap) {
    struct VMMDLL_MAP_PHYSMEM_impl {
        DWORD dwVersion;
        DWORD _Reserved1[5];
        DWORD cMap;
        DWORD _Reserved2;
        VMMDLL_MAP_PHYSMEMENTRY pMap[2];
    };

    PVMMDLL_MAP_PHYSMEM memMap = (VMMDLL_MAP_PHYSMEM *) new VMMDLL_MAP_PHYSMEM_impl();
    memMap->dwVersion = VMMDLL_MAP_PHYSMEM_VERSION;
    memMap->cMap = 2;

    VMMDLL_MAP_PHYSMEMENTRY entry1;
    entry1.pa = 0x123;
    entry1.cb = 0x133;
    VMMDLL_MAP_PHYSMEMENTRY entry2;
    entry2.pa = 0x213;
    entry2.cb = 0x432;

    memMap->pMap[0] = entry1;
    memMap->pMap[1] = entry2;

    std::vector<std::string> actualArgv, expectedArgv;

    expectedArgv.push_back("");
    expectedArgv.push_back("-v");
    expectedArgv.push_back("-printf");
    expectedArgv.push_back("-device");
    expectedArgv.push_back("fpga://algo=0");

    auto expectedHandle = (VMM_HANDLE) 1;

    EXPECT_CALL(*pcileech, VMMDLL_Initialize(_, _))
            .WillOnce(DoAll(Invoke([&actualArgv](DWORD argc, LPCSTR argv[]) {
                for (auto i = 0; i < argc; i++) {
                    auto s = std::string(argv[i]);
                    actualArgv.push_back(s);
                }
            }), Return(expectedHandle)));

    EXPECT_CALL(*pcileech, VMMDLL_MemFree(_));
    EXPECT_CALL(*pcileech, VMMDLL_Close(expectedHandle));

    EXPECT_CALL(*pcileech, VMMDLL_Map_GetPhysMem(_, _))
            .WillOnce(DoAll(Invoke([&memMap](VMM_HANDLE hVMM, PVMMDLL_MAP_PHYSMEM *ppPhysMemMap) {
                *ppPhysMemMap = memMap;
            }), Return(true)));

    EXPECT_TRUE(memory->generateMemoryMap());

    std::ifstream mmapFile(memory->mmapPath());
    ASSERT_TRUE(mmapFile);

    std::vector<std::string> lines;
    std::string line;
    while (getline(mmapFile, line)) {
        lines.push_back(line);
    }
    mmapFile.close();

    std::vector<std::string> expectedContents;
    expectedContents.push_back("0000  123  -  255  ->  123");
    expectedContents.push_back("0001  213  -  644  ->  213");

    ASSERT_EQ(lines, expectedContents);
    EXPECT_EQ(actualArgv, expectedArgv);
}
