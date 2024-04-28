#pragma once

#include <mutex>
#include <unordered_map>
#include <vector>
#include "Memory.h"

#include <gtest/gtest_prod.h>

typedef int CE_HANDLE;

class ClientState {
    FRIEND_TEST(ClientTest, Process32First);
    FRIEND_TEST(ClientTest, Module32First);

protected:
    std::mutex mtx;

    CE_HANDLE lastHandleID = 0;

    // used to store last position for process32first/next commands
    std::unordered_map<CE_HANDLE, int32_t> processIter;

    // used to store last position for module32first/next commands
    std::unordered_map<CE_HANDLE, int32_t> moduleIter;

    std::unordered_map<CE_HANDLE, std::vector<ProcessInfo> > handlesToProcessInfoSnapshots;
    std::unordered_map<CE_HANDLE, std::vector<MemoryRegion> > handlesToModuleSnapshots;
    std::unordered_map<CE_HANDLE, int32_t> handlesToProcessIDs;

    CE_HANDLE generateHandle();

public:
    ClientState();

    CE_HANDLE storeTlHelp32Snapshot(std::vector<ProcessInfo> snapshot);

    CE_HANDLE storeTlHelp32Snapshot(std::vector<ProcessInfo> snapshot, CE_HANDLE handle);

    CE_HANDLE storeTlHelp32Snapshot(std::vector<MemoryRegion> snapshot);

    CE_HANDLE storeTlHelp32Snapshot(std::vector<MemoryRegion> snapshot, CE_HANDLE handle);

    CE_HANDLE storeOpenProcess(int32_t processID);

    CE_HANDLE storeOpenProcess(int32_t processID, CE_HANDLE handle);

    void closeHandle(CE_HANDLE handle);

    ProcessInfo* nextProcessInfo(CE_HANDLE handle);

    MemoryRegion* nextMemoryRegion(CE_HANDLE handle);

    void resetProcessIterator(CE_HANDLE handle);

    void resetModuleIterator(CE_HANDLE handle);

    int getProcessID(CE_HANDLE handle);

    int getProcessHandleLegacyCE();

    bool containsProcessInfoHandle(CE_HANDLE handle);

    bool containsMemoryRegionHandle(CE_HANDLE handle);

    bool containsProcessIDHandle(CE_HANDLE handle);
};
