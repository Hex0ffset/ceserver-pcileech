#include "Memory.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <absl/log/log.h>

Memory::Memory(std::filesystem::path mmapPath,
               std::shared_ptr<PCILeechWrapper> pcileech) : mmapPath_(mmapPath.string()),
                                                            pcileech(pcileech) {
}

Memory::~Memory() {
    shutdown();
    if (hVMM) pcileech->VMMDLL_Close(hVMM);
}

std::string Memory::mmapPath() const {
    return mmapPath_;
}

bool Memory::generateMemoryMap() const {
    LOG(INFO) << "Generating memory map...";

    LPCSTR args[] = {(LPCSTR) "", (LPCSTR) "-v", (LPCSTR) "-printf", (LPCSTR) "-device", (LPCSTR) "fpga://algo=0"};
    int argc = sizeof(args) / sizeof(args[0]);

    VMM_HANDLE handle = pcileech->VMMDLL_Initialize(argc, args);
    if (!handle) {
        LOG(ERROR) << "pcileech->VMMDLL_Initialize failed" << std::endl;
        return false;
    }

    PVMMDLL_MAP_PHYSMEM pPhysMemMap = NULL;
    if (!pcileech->VMMDLL_Map_GetPhysMem(handle, &pPhysMemMap)) {
        LOG(ERROR) << "pcileech->VMMDLL_Map_GetPhysMem failed" << std::endl;
        pcileech->VMMDLL_Close(handle);
        return false;
    }

    if (pPhysMemMap->dwVersion != VMMDLL_MAP_PHYSMEM_VERSION) {
        LOG(ERROR) << "Invalid VMM Map Version" << std::endl;
        pcileech->VMMDLL_MemFree(pPhysMemMap);
        pcileech->VMMDLL_Close(handle);
        return false;
    }

    if (pPhysMemMap->cMap == 0) {
        LOG(ERROR) << "Failed to get physical memory map" << std::endl;
        pcileech->VMMDLL_MemFree(pPhysMemMap);
        pcileech->VMMDLL_Close(handle);
        return false;
    }

    std::stringstream sb;
    for (DWORD i = 0; i < pPhysMemMap->cMap; i++) {
        sb << std::setfill('0') << std::setw(4) << i << "  " << std::hex << pPhysMemMap->pMap[i].pa << "  -  " << (
                    pPhysMemMap->pMap[i].pa + pPhysMemMap->pMap[i].cb - 1) << "  ->  " << pPhysMemMap->pMap[i].pa <<
                std::endl;
    }

    std::ofstream mmapFile(mmapPath_);
    mmapFile << sb.str();
    mmapFile.flush();
    mmapFile.close();

    pcileech->VMMDLL_MemFree(pPhysMemMap);
    pcileech->VMMDLL_Close(handle);
    return true;
}

bool Memory::init() {
    if (!generateMemoryMap()) {
        LOG(ERROR) << "Failed to generate memory map" << std::endl;
        return false;
    }

    LPCSTR argv[] = {
        (LPCSTR) "-printf", (LPCSTR) "-v", (LPCSTR) "-device", (LPCSTR) "fpga", (LPCSTR) "-memmap",
        (LPCSTR) mmapPath_.c_str()
    };
    DWORD argc = sizeof(argv) / sizeof(LPCSTR);

    hVMM = pcileech->VMMDLL_Initialize(argc, argv);

    start();

    return hVMM != NULL;
}

void Memory::start() {
    if (dispatchThread == nullptr) {
        bStop = false;
        dispatchThread = new std::thread(&Memory::dispatchLoop, this);
    }
}

void Memory::shutdown() {
    bStop = true;
    backlogPush.notify_all();
    if (dispatchThread != nullptr) {
        if (dispatchThread->joinable()) dispatchThread->join();
        delete dispatchThread;
    }
}

std::vector<ProcessInfo> Memory::getProcessSnapshot() const {
    std::vector<ProcessInfo> procInfos;
    size_t numProcs = 0;

    if (!pcileech->VMMDLL_PidList(hVMM, NULL, &numProcs)) {
        LOG(ERROR) << "Failed to get number of process IDs" << std::endl;

        return procInfos;
    }
    PDWORD ptrProcessIDs = (PDWORD) malloc(numProcs * sizeof(DWORD));
    if (!pcileech->VMMDLL_PidList(hVMM, ptrProcessIDs, &numProcs)) {
        LOG(ERROR) << "Failed to get list of process IDs" << std::endl;

        return procInfos;
    }

    for (int i = 0; i < numProcs; i++) {
        auto pid = ptrProcessIDs[i];
        auto name = pcileech->VMMDLL_ProcessGetInformationString(
            hVMM, pid, VMMDLL_PROCESS_INFORMATION_OPT_STRING_PATH_USER_IMAGE);

        if (name != NULL) procInfos.push_back(ProcessInfo(name, pid));
    }

    pcileech->VMMDLL_MemFree(ptrProcessIDs);

    return procInfos;
}

std::vector<MemoryRegion> Memory::getModuleSnapshot(int processID) const {
    std::vector<MemoryRegion> regions;

    PVMMDLL_MAP_VAD memoryMap = nullptr;
    // VAD == Virtual Address Descriptor
    if (!pcileech->VMMDLL_Map_GetVadU(hVMM, processID, true, &memoryMap)) {
        LOG(ERROR) << "Failed to get Virtual Address Descriptor for PID " << processID << std::endl;

        return regions;
    }

    for (int i = 0; i < memoryMap->cMap; i++) {
        regions.push_back(MemoryRegion(memoryMap->pMap[i]));
    }

    pcileech->VMMDLL_MemFree(memoryMap);

    return regions;
}

std::vector<ThreadInfo> Memory::getThreadSnapshot(int processID) const {
    std::vector<ThreadInfo> threads;

    PVMMDLL_MAP_THREAD threadMap = nullptr;
    if (!pcileech->VMMDLL_Map_GetThread(hVMM, processID, &threadMap)) {
        LOG(ERROR) << "Failed ot get therad map for PID " << processID << std::endl;
        return threads;
    }

    for (int i = 0; i < threadMap->cMap; i++) {
        threads.push_back(ThreadInfo(threadMap->pMap[i]));
    }

    pcileech->VMMDLL_MemFree(threadMap);

    return threads;
}

size_t Memory::read(char *buffer, int32_t processID, uint64_t address, size_t size) {
    mtx.lock();

    std::shared_ptr<BacklogItem> item = nullptr;
    std::string key = std::to_string(processID) + ":" + std::to_string(address) + ":" + std::to_string(size);
    if (backlog.contains(key)) {
        item = backlog[key];
    } else {
        item = std::make_shared<BacklogItem>(processID, address, size);
        backlog[key] = item;
        backlogPush.notify_all();
    }
    mtx.unlock();

    std::pair<size_t, const char *> result = item->get();
    memcpy(buffer, result.second, result.first);

    return result.first;
}

size_t Memory::write(const char *buffer, int32_t processID, uint64_t address, size_t size) {
    sem.acquire();

    if (!pcileech->VMMDLL_MemWrite(hVMM, processID, (ULONG64) address, (PBYTE) buffer, (DWORD) size)) {
        sem.release();
        return -1;
    }

    sem.release();

    return size;
}

MemoryModel Memory::getMemoryModel(int processID) const {
    VMMDLL_PROCESS_INFORMATION procInfo;
    auto procInfoSize = sizeof(procInfo);
    memset(&procInfo, 0, sizeof(VMMDLL_PROCESS_INFORMATION));
    procInfo.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
    procInfo.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;

    if (!pcileech->VMMDLL_ProcessGetInformation(hVMM, processID, &procInfo, &procInfoSize)) {
        LOG(ERROR) << "Failed to get process information data" << std::endl;
        return MemoryModel::MemoryModel_UNKNOWN;
    }

    MemoryModel model;
    switch (procInfo.tpMemoryModel) {
        case VMMDLL_MEMORYMODEL_NA:
            model = MemoryModel::MemoryModel_NA;
            break;
        case VMMDLL_MEMORYMODEL_X86PAE:
            model = MemoryModel::MemoryModel_X86PAE;
            break;
        case VMMDLL_MEMORYMODEL_ARM64:
            model = MemoryModel::MemoryModel_ARM64;
            break;
        case VMMDLL_MEMORYMODEL_X64:
            model = MemoryModel::MemoryModel_X64;
            break;
        case VMMDLL_MEMORYMODEL_X86:
            model = MemoryModel::MemoryModel_X86;
            break;
        default:
            model = MemoryModel::MemoryModel_UNKNOWN;
            break;
    }

    return model;
}

void Memory::dispatchLoop() {
    VLOG(1) << "Dispatch loop started";

    auto nextDispatchTime = Clock::now();
    auto lastDispatchTime = Clock::now();

    while (!bStop) {
        mtx.lock();
        if (backlog.empty()) {
            mtx.unlock();
            std::unique_lock<std::mutex> lock(mtx);
            backlogPush.wait(lock, [&]() { return bStop || backlog.size() > 0; });
            continue;
        }

        mtx.unlock();
        while (backlog.size() < MAX_DISPATCH_SIZE && nextDispatchTime > Clock::now() && Clock::now() - lastDispatchTime
               < std::chrono::nanoseconds(DMA_MAX_BACKLOG_BUILD_TIME_NS)) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(DMA_MAX_BACKLOG_BUILD_TIME_NS));
        }
        mtx.lock();

        Backlog dispatchBuffer;
        int pid = backlog.begin()->second->pid;
        size_t requestedSoFar = 0;
        for (auto &item: backlog) {
            if (item.second->pid != pid) continue;

            // we can only read up to 1G at a time
            if (requestedSoFar + item.second->size >= MAX_BYTES_PER_DISPATCH) break;

            dispatchBuffer[item.first] = item.second;

            requestedSoFar += item.second->size;

            if (dispatchBuffer.size() >= MAX_DISPATCH_SIZE) break;
        }
        for (auto &item: dispatchBuffer) {
            backlog.erase(item.first);
        }

        lastDispatchTime = Clock::now();
        nextDispatchTime = lastDispatchTime + std::chrono::nanoseconds(DMA_BACKOFF_TIMER_EXTEND_NS);

        mtx.unlock();

        std::async(std::launch::async, [this, &pid, &dispatchBuffer]() {
            sem.acquire();
            dispatch(pid, dispatchBuffer);
            sem.release();
        });
    }
}

void Memory::dispatch(int pid, Backlog &snapshot) {
    if (snapshot.size() <= SCATTER_READ_THRESHOLD) {
        // sequential read
        VLOG(10) << "Performing sequential read for " << snapshot.size() << " request(s)";
        for (auto &item: snapshot) {
            char *promiseBuffer = new char[item.second->size];
            memset(promiseBuffer, 0, item.second->size);
            DWORD read = 0;

            if (!pcileech->VMMDLL_MemReadEx(hVMM, pid, (ULONG64) item.second->address, (PBYTE) promiseBuffer,
                                            (DWORD) item.second->size, &read, scatterFlags)) {
                LOG(ERROR) << "Failed to sequentially read " << item.second->size << " bytes at " << std::hex << item.
second->address;
            }

            item.second->promise.set_value({read, promiseBuffer});

            pcileech->VMMDLL_MemFree(promiseBuffer);
        }
    } else {
        // scatter read
        VLOG(10) << "Performing scatter read for " << snapshot.size() << " request(s)";
        VMMDLL_SCATTER_HANDLE hndScatter = pcileech->VMMDLL_Scatter_Initialize(hVMM, pid, scatterFlags);

        for (auto &item: snapshot) {
            if (!pcileech->VMMDLL_Scatter_Prepare(hndScatter, (QWORD) (item.second->address), item.second->size)) {
                LOG(ERROR) << "Failed to prepare scatter read for " << item.second->size << " bytes at "
                    << std::hex << item.second->address;
            }
        }

        if (!pcileech->VMMDLL_Scatter_Execute(hndScatter)) {
            LOG(ERROR) << "Failed to execute scatter read";
        }

        for (auto &item: snapshot) {
            char *promiseBuffer = new char[item.second->size];
            memset(promiseBuffer, 0, item.second->size);
            DWORD read = 0;
            if (!pcileech->VMMDLL_Scatter_Read(hndScatter, (QWORD) item.second->address, item.second->size,
                                               (PBYTE) promiseBuffer,
                                               &read)) {
                LOG(ERROR) << "Failed to scatter read " << item.second->size << " bytes at " << std::hex << item.second
->address;
            }

            item.second->promise.set_value({read, promiseBuffer});
        }

        //pcileech->VMMDLL_Scatter_Clear(hndScatter, 0, VMMDLL_FLAG_NOCACHE);
        pcileech->VMMDLL_Scatter_CloseHandle(hndScatter);
    }
}
