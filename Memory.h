#pragma once

#include <mutex>
#include <vector>
#include <locale>
#include <codecvt>

#ifdef _WIN32
#pragma comment(lib, "vmm")
#endif

#include <filesystem>
#include <unordered_map>
#include <gtest/gtest_prod.h>

#include "PCILeechWrapper.h"
#include <thread>
#include <future>
#include <cstring>
#include <semaphore>

const int NANOSECONDS_IN_A_MILLISECOND = 1000000;

const int BYTES_IN_A_GIGABYTE = 1000000000; // go for giga, not gibi to be safe

struct ProcessInfo {
    std::string name;
    int32_t pid;

    ProcessInfo(std::string name, int32_t pid) : name(name), pid(pid) {
    };
};

struct ThreadInfo {
    VMMDLL_MAP_THREADENTRY threadentry;

    ThreadInfo(VMMDLL_MAP_THREADENTRY threadentry) : threadentry(threadentry) {
    };
};

struct MemoryRegion {
    VMMDLL_MAP_VADENTRY vadEntry;
    uint64_t startAddr;
    int32_t regionSize;
    uint32_t fileOffset;
    int32_t part;
    int protection;
    bool privateMemory;
    int vadType;
    std::string name;

    MemoryRegion(VMMDLL_MAP_VADENTRY vadEntry) : startAddr(vadEntry.vaStart),
                                                 vadEntry(vadEntry),
                                                 regionSize(vadEntry.vaEnd - vadEntry.vaStart + 1),
                                                 fileOffset(0),
                                                 part(0),
                                                 protection(vadEntry.Protection),
                                                 privateMemory(vadEntry.fPrivateMemory),
                                                 vadType(vadEntry.VadType) {
        if (vadEntry.uszText != NULL) {
            name = std::string(vadEntry.uszText);
        } else {
            name = "";
        }

        if (name.empty()) {
            if (vadEntry.fHeap) {
                name = "<heap>";
            } else if (vadEntry.fStack) {
                name = "<stack>";
            } else if (vadEntry.fPageFile) {
                name = "<pagefile>";
            } else if (vadEntry.fFile) {
                name = "<file>";
            } else if (vadEntry.fImage) {
                name = "<image>";
            } else if (vadEntry.fTeb) {
                name = "<teb>";
            } else {
                name = "<unknown>";
            }
        }
    }
};

enum class MemoryModel {
    MemoryModel_UNKNOWN = 0,
    MemoryModel_NA = 1,
    MemoryModel_X86PAE = 2,
    MemoryModel_ARM64 = 3,
    MemoryModel_X64 = 4,
    MemoryModel_X86 = 5
};

inline std::unordered_map<MemoryModel, std::string> MemoryModelToString = {
    {MemoryModel::MemoryModel_UNKNOWN, "UNKNOWN"},
    {MemoryModel::MemoryModel_NA, "NA"},
    {MemoryModel::MemoryModel_X86PAE, "X86PAE"},
    {MemoryModel::MemoryModel_ARM64, "ARM64"},
    {MemoryModel::MemoryModel_X64, "X64"},
    {MemoryModel::MemoryModel_X86, "X86"}
};

struct BacklogItem {
    int pid;
    uint64_t address;
    size_t size;
    bool promiseRead = false;
    size_t read = 0;
    char *data = nullptr;
    std::mutex mtx;
    std::promise<std::pair<size_t, const char *> > promise;

    BacklogItem(int32_t processID, uint64_t address, size_t size) : pid(processID), address(address), size(size) {
        data = new char[size];
    };

    ~BacklogItem() {
        if (data != nullptr) delete data;
    }

    std::pair<size_t, const char *> get() {
        std::lock_guard<std::mutex> lock(mtx);

        if (!promiseRead) {
            auto result = promise.get_future().get();
            read = result.first;
            memcpy(data, result.second, size);
            promiseRead = true;
        }
        return {read, data};
    }
};

class Memory {
    FRIEND_TEST(MemoryTest, ReadMemoryScatter);

    using BacklogKey = std::string;
    using Backlog = std::unordered_map<BacklogKey, std::shared_ptr<BacklogItem> >;
    using Clock = std::chrono::high_resolution_clock;

    friend class ClientTest;

    FRIEND_TEST(ClientTest, CreateAndReadProcessSnapshot);

private:
    static const inline uint32_t DMA_MAX_BACKLOG_BUILD_TIME_NS = 2 * NANOSECONDS_IN_A_MILLISECOND; // 3000;

    static const inline uint32_t DMA_BACKOFF_TIMER_EXTEND_NS = NANOSECONDS_IN_A_MILLISECOND / 2;

    static const inline uint32_t MAX_DISPATCH_SIZE = 256;

    static const inline uint32_t MAX_BYTES_PER_DISPATCH = BYTES_IN_A_GIGABYTE;

    static const inline int SCATTER_READ_THRESHOLD = 1; // <= this value uses VMMDLL_MemReadEx, else scatter read

    static inline std::binary_semaphore sem{1}; // used to control max concurrent operations on the DMA device

    static inline auto scatterFlags = VMMDLL_FLAG_NOCACHE |
                                      VMMDLL_FLAG_NOCACHEPUT |
                                      VMMDLL_FLAG_ZEROPAD_ON_FAIL;

    VMM_HANDLE hVMM = NULL;

    std::string mmapPath_;

    std::shared_ptr<PCILeechWrapper> pcileech;

    std::thread *dispatchThread = nullptr;

    bool bStop = false;

    std::mutex mtx;

    std::condition_variable backlogPush;

    Backlog backlog;

    void dispatchLoop();

    void dispatch(int pid, Backlog &snapshot);

public:
    Memory(std::filesystem::path mmapPath, std::shared_ptr<PCILeechWrapper> pcileech);

    ~Memory();

    virtual bool generateMemoryMap() const;

    std::string mmapPath() const;

    virtual bool init();

    void start();

    inline void shutdown();

    virtual std::vector<ProcessInfo> getProcessSnapshot() const;

    virtual std::vector<MemoryRegion> getModuleSnapshot(int processID) const;

    virtual std::vector<ThreadInfo> getThreadSnapshot(int processID) const;

    virtual size_t read(char *buffer, int32_t processID, uint64_t address, size_t size);

    virtual size_t write(const char *buffer, int32_t processID, uint64_t address, size_t size);

    virtual MemoryModel getMemoryModel(int processID) const;
};
