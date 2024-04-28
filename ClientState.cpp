#include "ClientState.h"
#include "Memory.h"

ClientState::ClientState() {
}

CE_HANDLE ClientState::generateHandle() {
    std::lock_guard<std::mutex> lock(mtx);

    lastHandleID++;
    if (lastHandleID >= UINT64_MAX || lastHandleID < 0) lastHandleID = 0;

    return lastHandleID;
}

CE_HANDLE ClientState::storeOpenProcess(int32_t processID) {
    return storeOpenProcess(processID,  generateHandle());
}

CE_HANDLE ClientState::storeOpenProcess(int32_t processID, CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    handlesToProcessIDs[handle] = processID;

    return handle;
}

CE_HANDLE ClientState::storeTlHelp32Snapshot(std::vector<ProcessInfo> snapshot) {
    return storeTlHelp32Snapshot(snapshot, generateHandle());
}

CE_HANDLE ClientState::storeTlHelp32Snapshot(std::vector<ProcessInfo> snapshot, CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    handlesToProcessInfoSnapshots.insert({handle, snapshot});
    processIter[handle] = 0;

    return handle;
}

CE_HANDLE ClientState::storeTlHelp32Snapshot(std::vector<MemoryRegion> snapshot) {
    return storeTlHelp32Snapshot(snapshot, generateHandle());
}

CE_HANDLE ClientState::storeTlHelp32Snapshot(std::vector<MemoryRegion> snapshot, CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    handlesToModuleSnapshots.insert({handle, snapshot});
    moduleIter[handle] = 0;

    return handle;
}

void ClientState::closeHandle(CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    handlesToProcessInfoSnapshots.erase(handle);
    handlesToModuleSnapshots.erase(handle);
    processIter.erase(handle);
    moduleIter.erase(handle);
    handlesToProcessIDs.erase(handle);
}

ProcessInfo* ClientState::nextProcessInfo(CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    if (!handlesToProcessInfoSnapshots.contains(handle) || processIter[handle] >= handlesToProcessInfoSnapshots[handle].size()) {
        return nullptr;
    }
    auto &procInfo = handlesToProcessInfoSnapshots[handle][processIter[handle]];
    processIter[handle]++;

    return &procInfo;
}

MemoryRegion* ClientState::nextMemoryRegion(CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    if (!handlesToModuleSnapshots.contains(handle) || moduleIter[handle] >= handlesToModuleSnapshots[handle].size()) {
        return nullptr;
    }
    auto &region = handlesToModuleSnapshots[handle][moduleIter[handle]];
    moduleIter[handle]++;

    return &region;
}

void ClientState::resetProcessIterator(CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    processIter[handle] = 0;
}

void ClientState::resetModuleIterator(CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    moduleIter[handle] = 0;
}

int ClientState::getProcessID(CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    return handlesToProcessIDs.contains(handle) ? handlesToProcessIDs[handle] : -1;
}

CE_HANDLE ClientState::getProcessHandleLegacyCE() {
    std::lock_guard<std::mutex> lock(mtx);

    if (handlesToProcessIDs.empty()) {
        return -1;
    }

    return handlesToProcessIDs.begin()->second;
}

bool ClientState::containsProcessInfoHandle(CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    return handlesToProcessInfoSnapshots.contains(handle);
}

bool ClientState::containsMemoryRegionHandle(CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    return handlesToModuleSnapshots.contains(handle);
}

bool ClientState::containsProcessIDHandle(CE_HANDLE handle) {
    std::lock_guard<std::mutex> lock(mtx);

    return handlesToProcessIDs.contains(handle);
}