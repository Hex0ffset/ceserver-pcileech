#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../../Memory.h"
#include <cstdio>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

class MockMemory : public Memory {
public:
    MockMemory() : Memory(createTempFile(), nullptr) {
    };

    MockMemory(std::shared_ptr<PCILeechWrapper> pcileech) : Memory(createTempFile(), pcileech) {
    };

    std::filesystem::path createTempFile() {
        std::string path = (std::filesystem::current_path() / "ceserver-pcileech.tests.XXXXXX").string();

#ifdef _WIN32
        auto tmp_path = _mktemp(const_cast<char *>(path.c_str()));
        if (tmp_path) {
            return std::filesystem::path(tmp_path);
        }
#else
        char* tmp_path = const_cast<char*>(path.c_str());
        int fd = mkstemp(const_cast<char *>(tmp_path));
        if (fd != -1) {
            close(fd);
            return std::filesystem::path(tmp_path);
        }
#endif

        std::cout << "Failed to create temporary file." << std::endl;
        return (std::filesystem::current_path() / "mmap.txt");
    }


    MOCK_METHOD(std::vector<ProcessInfo>, getProcessSnapshot, (), (const, override));

    MOCK_METHOD(std::vector<MemoryRegion>, getModuleSnapshot, (int), (const, override));

    MOCK_METHOD(std::vector<ThreadInfo>, getThreadSnapshot, (int), (const, override));

    MOCK_METHOD(size_t, read, (char*, int32_t, uint64_t, size_t), (override));

    MOCK_METHOD(size_t, write, (const char*, int32_t, uint64_t, size_t), (override));

    MOCK_METHOD(bool, generateMemoryMap, (), (const, override));

    MOCK_METHOD(MemoryModel, getMemoryModel, (int), (const, override));
};
