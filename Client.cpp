#include "Client.h"
#include "Commands.h"
#include "CommandsToString.h"
#include <iostream>
#include "Memory.h"
#include <algorithm>
#include <thread>
#include <absl/log/log.h>

Client::Client(struct sockaddr_in clientInfo, std::shared_ptr<SocketWrapper> clientSocket,
               Memory *memory, ClientState *state) : client(clientInfo),
                                                     clientSocket(clientSocket), memory(memory), state_(state) {
    std::thread::id tid = std::this_thread::get_id();
    std::ostringstream oss;
    oss << tid;
    threadId = oss.str();
}

Client::~Client() {
}

ClientState *Client::state() const {
    return state_;
}


std::string Client::getClientAddress() const {
    return getClientAddress(client);
}

std::string Client::getClientAddress(struct sockaddr_in client) {
    char clientIP[INET_ADDRSTRLEN]{0};
    inet_ntop(AF_INET, &(client.sin_addr), clientIP, INET_ADDRSTRLEN);

    return std::string(clientIP);
}

void Client::handleUnknownCommand(unsigned char command) const {
    // currently there's no response to signal an unknown/unimplemented command,
    // but we'll reserve this function for future use in case one is added
    LOG(ERROR) << "Received unknown command: " << (unsigned int) command;
}

void Client::handleKnownButUnimplementedCommand(unsigned char command) const {
    // currently there's no response to signal an unknown/unimplemented command,
    // but we'll reserve this function for future use in case one is added
    LOG(ERROR) << "Received known, but unimplemented command: " << (unsigned int) command
        << " (" << commandToString[command] << ")";
}

bool Client::dispatchCommand(unsigned char command) {
    // we're locking while receiving on the socket, but that's fine for now since all commands are serial anyway
    std::lock_guard<std::mutex> lock(mtx);

    auto identifier = threadId + " " + connectionName_;
    if (identifier == "") {
        identifier = "<anonymous>";
    }

    auto verbosity = 1;
    if (command == CMD_READPROCESSMEMORY) verbosity = 10; // don't spam the logs with this as it's heavily used
    if (command == CMD_WRITEPROCESSMEMORY) verbosity = 9; // similar story when a value is frozen

    if (commandToString.contains(command)) {
        VLOG(verbosity) << "**** [" << std::setw(40) << std::left << identifier << "] Handling command: " <<
 std::setw(0)
            << (int) command << " (" << commandToString[command] << ")";
    } else {
        VLOG(verbosity) << "**** [" << std::setw(40) << std::left << identifier << "] Handling command: " <<
 std::setw(0)
            << (int) command;
    }

    auto keepSocketOpen = true;

    switch (command) {
        case CMD_CREATETOOLHELP32SNAPSHOT:
            handleCommandToolHelp32Snapshot();
            break;
        case CMD_PROCESS32FIRST:
        case CMD_PROCESS32NEXT:
            handleCommandIterateProcessesNext(command);
            break;
        case CMD_MODULE32FIRST:
        case CMD_MODULE32NEXT:
            handleCommandIterateModulesNext(command);
            break;
        case CMD_CLOSEHANDLE:
            handleCommandCloseHandle();
            break;
        case CMD_GETVERSION:
            handleCommandGetVersion();
            break;
        case CMD_GETABI:
            handleCommandGetABI();
            break;
        case CMD_CREATETOOLHELP32SNAPSHOTEX:
            handleCommandToolHelp32SnapshotEx();
            break;
        case CMD_SET_CONNECTION_NAME:
            handleCommandSetConnectionName();
            break;
        case CMD_CLOSECONNECTION:
            keepSocketOpen = false;
            break;
        case CMD_TERMINATESERVER:
            keepSocketOpen = false; // handled in Server.cpp logic
            break;
        case CMD_OPENPROCESS:
            handleCommandOpenProcess();
            break;
        case CMD_READPROCESSMEMORY:
            keepSocketOpen = handleCommandReadProcessMemory();
            break;
        case CMD_WRITEPROCESSMEMORY:
            keepSocketOpen = handleCommandWriteProcessMemory();
            break;
        case CMD_GETARCHITECTURE:
            keepSocketOpen = handleCommandGetArchitecture();
            break;
        case CMD_GETSYMBOLLISTFROMFILE:
            handleCommandGetSymbolListFromFile();
            break;
        case CMD_VIRTUALQUERYEX:
        case CMD_GETREGIONINFO:
            keepSocketOpen = handleCommandGetRegionInfo(command);
            break;
        case CMD_VIRTUALQUERYEXFULL:
            return handleCommandVirtualQueryExFull();
        case CMD_STARTDEBUG:
            handleCommandStartDebug();
            break;
        case CMD_GETOPTIONS:
            handleCommandGetOptions();
            break;
        case CMD_STOPDEBUG:
        case CMD_WAITFORDEBUGEVENT:
        case CMD_CONTINUEFROMDEBUGEVENT:
        case CMD_SETBREAKPOINT:
        case CMD_REMOVEBREAKPOINT:
        case CMD_SUSPENDTHREAD:
        case CMD_RESUMETHREAD:
        case CMD_GETTHREADCONTEXT:
        case CMD_SETTHREADCONTEXT:
        case CMD_LOADEXTENSION:
        case CMD_ALLOC:
        case CMD_FREE:
        case CMD_CREATETHREAD:
        case CMD_LOADMODULE:
        case CMD_SPEEDHACK_SETSPEED:
        case CMD_CHANGEMEMORYPROTECTION:
        case CMD_GETOPTIONVALUE:
        case CMD_SETOPTIONVALUE:
        case CMD_PTRACE_MMAP:
        case CMD_OPENNAMEDPIPE:
        case CMD_PIPEREAD:
        case CMD_PIPEWRITE:
        case CMD_GETCESERVERPATH:
        case CMD_ISANDROID:
        case CMD_LOADMODULEEX:
        case CMD_SETCURRENTPATH:
        case CMD_GETCURRENTPATH:
        case CMD_ENUMFILES:
        case CMD_GETFILEPERMISSIONS:
        case CMD_SETFILEPERMISSIONS:
        case CMD_GETFILE:
        case CMD_PUTFILE:
        case CMD_CREATEDIR:
        case CMD_DELETEFILE:
        case CMD_AOBSCAN:
        case CMD_COMMANDLIST2:
            handleKnownButUnimplementedCommand(command);
            keepSocketOpen = false;
            break;

        default:
            handleUnknownCommand(command);
            keepSocketOpen = false;
            break;
    }

    if (commandToString.contains(command)) {
        VLOG(verbosity) << "**** [" << std::setw(40) << std::left << identifier << "] Finished handling command: " <<
 std::setw(0) << (int) command << " (" << commandToString[command] << ")";
    } else {
        VLOG(verbosity) << "**** [" << std::setw(40) << std::left << identifier << "] Finished handling command: " <<
 std::setw(0) << (int)
 command;
    }

    return keepSocketOpen;
}

template<typename T>
T Client::read() const {
    return read<T>(MSG_WAITALL);
}

template<typename T>
T Client::read(int flags) const {
    return clientSocket->recv<T>(flags);
}

int Client::read(char *buffer, size_t size) const {
    return read(buffer, size, MSG_WAITALL);
}

int Client::read(char *buffer, size_t size, int flags) const {
    return clientSocket->recv(buffer, size, flags);
}

template<typename T>
size_t Client::send(T data) const {
    return send((char *) &data, sizeof(T));
}

size_t Client::send(const char *buffer, size_t size) const {
    return send(buffer, size, 0);
}

size_t Client::send(const char *buffer, size_t size, int flags) const {
    return clientSocket->send(buffer, size, flags);
}

void Client::handleCommandToolHelp32Snapshot() {
    auto flags = read<DWORD>();
    auto procId = read<DWORD>();

    CE_HANDLE handle = 0;

    if (flags & TH32CS_SNAPPROCESS) {
        VLOG(1) << "Creating process snapshot";
        auto snapshot = memory->getProcessSnapshot();
        VLOG(1) << "Storing process snapshot";
        handle = state_->storeTlHelp32Snapshot(snapshot);
    } else if (flags & TH32CS_SNAPMODULE) {
        VLOG(1) << "Creating module snapshot";
        auto snapshot = memory->getModuleSnapshot(procId);
        VLOG(1) << "Storing module snapshot";
        handle = state_->storeTlHelp32Snapshot(snapshot);
    } else {
        LOG(ERROR) << "Unknown flag(s) passed to CMD_CREATETOOLHELP32SNAPSHOT";
    }
    VLOG(1) << "CMD_CREATETOOLHELP32SNAPSHOT handle: " << handle;
    send<CE_HANDLE>(handle);
}

void Client::handleCommandToolHelp32SnapshotEx() {
    auto flags = read<DWORD>();
    auto procId = read<DWORD>();

    if (flags & TH32CS_SNAPTHREAD) {
        VLOG(1) << "Creating thread snapshot for PID " << procId;
        auto snapshot = memory->getThreadSnapshot(procId);

        VLOG(1) << "Sending thread list";
        send<int>(snapshot.size());
        for (auto &thread: snapshot) {
            sendThread32Entry(thread.threadentry.dwTID);
        }
        VLOG(1) << "Done sending thread list (size=" << snapshot.size() << ")";
        return;
    }

    if (flags & TH32CS_SNAPMODULE) {
        VLOG(1) << "Creating module snapshot for PID " << procId;
        auto snapshot = memory->getModuleSnapshot(procId);

        VLOG(1) << "Sending module list (size=" << snapshot.size() << ")";
        for (auto &memoryRegion: snapshot) {
            sendModule32Entry(true, memoryRegion.startAddr, memoryRegion.part, memoryRegion.regionSize,
                              memoryRegion.fileOffset, memoryRegion.name);
        }

        // send end of list
        sendModule32Entry(false, 0, 0, 0, 0, "");
        VLOG(1) << "Done sending module list";
        return;
    }

    // CE hasn't implemented the rest yet, so it falls back to normal process32next behaviour
    auto snapshot = memory->getProcessSnapshot();
    auto handle = state_->storeTlHelp32Snapshot(snapshot);
    printf("CMD_CREATETOOLHELP32SNAPSHOTEX handle: %d\n", handle);
    send<CE_HANDLE>(handle);
}

void Client::sendProcess32Entry(bool hasNext, int32_t processID, std::string name) const {
    VLOG(2) << "Sending process entry for '" << name << "' PID: " << processID;

    auto entry = CeProcessEntry{};
    entry.result = hasNext ? 1 : 0;
    entry.pid = processID;
    entry.processnamesize = name.size();
    send<CeProcessEntry>(entry);
    if (!name.empty()) {
        send(name.c_str(), name.size());
    }
}

void Client::sendModule32Entry(bool hasNext, uint64_t moduleBase, int32_t modulePart, int32_t memoryRegionSize,
                               uint32_t fileOffset, std::string name) const {
    LOG(INFO) << "Sending module entry for " << (name.size() ? name : "<empty>")
            << " " << std::hex << moduleBase
            << " part " << std::dec << modulePart
            << " size " << std::hex << memoryRegionSize
            //<< " offset " << std::hex << fileOffset
            << " " << (name.empty() ? "(without name)" : "(with name size " + std::to_string(name.size()) + ")");

    auto entry = CeModuleEntry{};
    entry.result = hasNext ? 1 : 0;
    entry.modulebase = moduleBase;
    entry.modulepart = modulePart;
    entry.modulesize = memoryRegionSize;
    //entry.modulefileoffset = fileOffset;
    entry.modulenamesize = name.size();
    send<CeModuleEntry>(entry);
    if (!name.empty()) {
        send(name.c_str(), name.size());
    }
}

void Client::sendThread32Entry(int threadID) const {
    send<int>(threadID);
}

void Client::handleCommandIterateProcessesNext(unsigned char command) {
    auto handle = read<CE_HANDLE>();

    auto sendEndOfList = [this]() {
        VLOG(1) << "Sending end of process list";
        sendProcess32Entry(false, 0, "");
    };

    if (!state_->containsProcessInfoHandle(handle)) {
        LOG(ERROR) << "Handle " << handle << " process snapshot doesn't exist!";
        sendEndOfList();
        return;
    }

    if (command == CMD_PROCESS32FIRST) state_->resetProcessIterator(handle);

    auto procInfo = state_->nextProcessInfo(handle);
    if (procInfo == nullptr) {
        sendEndOfList();
        return;
    }
    auto name = (char *) procInfo->name.c_str();
    sendProcess32Entry(true, procInfo->pid, name);
}

void Client::handleCommandIterateModulesNext(unsigned char command) {
    auto handle = read<CE_HANDLE>();

    auto sendEndOfList = [this]() {
        VLOG(1) << "Sending end of module list";
        sendModule32Entry(false, 0, 0, 0, 0, "");
    };

    if (!state_->containsMemoryRegionHandle(handle)) {
        LOG(ERROR) << "Handle " << handle << " module snapshot doesn't exist!";
        // we can't auto-recreate this one as we need the PID to do so
        sendEndOfList();
        return;
    }

    if (command == CMD_MODULE32FIRST) state_->resetModuleIterator(handle);

    auto memoryRegion = state_->nextMemoryRegion(handle);
    if (memoryRegion == nullptr) {
        sendEndOfList();
        return;
    }

    sendModule32Entry(true, memoryRegion->startAddr, memoryRegion->part, memoryRegion->regionSize,
                      memoryRegion->fileOffset, memoryRegion->name);
}

void Client::handleCommandCloseHandle() {
    auto handle = read<CE_HANDLE>();

    VLOG(1) << "Closing handle: " << handle;

    state_->closeHandle(handle);

    send<int>(1);
}

void Client::handleCommandGetVersion() {
    auto version = CeVersion{};
    version.version = CE_SERVER_VERSION;
    version.stringsize = VERSION_STRING.size();
    send<CeVersion>(version);
    send((char *) VERSION_STRING.c_str(), VERSION_STRING.size());
    VLOG(1) << "Server version " << CE_SERVER_VERSION << " (" << VERSION_STRING.c_str() << ")";
}

void Client::handleCommandGetABI() {
    // MemProcFS only supports reading Windows memory currently
    send<unsigned char>(ABI_WINDOWS);
}

void Client::handleCommandSetConnectionName() {
    auto nameLength = read<uint32_t>();
    auto name = new char[nameLength + 1];
    read(name, nameLength);
    name[nameLength] = 0;

    connectionName_ = std::string(name);
    VLOG(1) << "New connection name: " << connectionName_;
}

std::string Client::connectionName() const {
    return connectionName_;
}

void Client::handleCommandOpenProcess() {
    DWORD pid = read<DWORD>();

    int handle = state_->storeOpenProcess(pid);

    VLOG(1) << "Process open handle: " << handle;
    send<int>(handle);
}

bool Client::handleCommandReadProcessMemory() {
    auto handle = read<CE_HANDLE>();
    auto address = read<uint64_t>();
    auto size = read<uint32_t>();
    auto compress = read<uint8_t>();

    if (compress) {
        LOG(ERROR) << "Compression not yet supported";
        return false; // close the socket
    }

    if (!state_->containsProcessIDHandle(handle)) {
        LOG(ERROR) << "Handle " << handle << " has not opened any processes";
        return false;
    }

    if (address <= 0) {
        // this seems to be normal for CE to request for some reason
        //LOG(WARNING) << "Invalid address requested: " << std::hex << address;
        send<int>(0);
        return true;
    }

    if (size <= 0) {
        LOG(ERROR) << "Invalid read size requested: " << size;
        send<int>(0);
        return true;
    }

    char *memoryContents = new char[size];
    VLOG(10) << "Reading " << size << " bytes of memory at " << std::hex << address;
    auto read = memory->read(memoryContents, state_->getProcessID(handle), address, size);
    if (read <= 0) {
        // read failed
        LOG(ERROR) << "Reading " << size << " bytes of memory at " << std::hex << address << " failed!"
            << std::dec << " (Read = " << read << " of " << size << " bytes)";
        send<int>(0);
        delete memoryContents;

        return true; // keep the socket open even though read failed (may be out of range, thus getting 0 byte read)
    }
    VLOG(10) << "Successfully read " << read << " bytes (requested " << size << ")";
    if (read < size) {
        LOG(WARNING) << "Only read = " << read << " of " << size << " bytes at " << std::hex << address;
    }

    send<int>(read);
    send(memoryContents, read);

    delete memoryContents;

    return true;
}

bool Client::handleCommandWriteProcessMemory() {
    auto handle = read<CE_HANDLE>();
    auto address = read<uint64_t>();
    auto size = read<int>();
    auto buffer = new char[size];
    read(buffer, size);

    if (!state_->containsProcessIDHandle(handle)) {
        LOG(ERROR) << "Handle " << handle << " has not opened any processes";
        return false;
    }

    VLOG(3) << "Writing " << size << " bytes of memory at " << std::hex << address;
    auto written = memory->write(buffer, state_->getProcessID(handle), address, size);

    if (written < 0) {
        // read failed
        LOG(ERROR) << "Write failed";
        send<int>(0);
        delete buffer;

        return false;
    }

    delete buffer;

    send<int32_t>(written);

    return written == size;
}

bool Client::handleCommandGetArchitecture() {
    CE_HANDLE handle = -1;
    int result = -1;
    auto timeout = std::chrono::system_clock::now() + std::chrono::milliseconds(1000);
    do {
        result = clientSocket->recvNonBlocking((char *) &handle, sizeof(CE_HANDLE), 0);
        if (result > 0) break;

        if (std::chrono::system_clock::now() >= timeout) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (result <= 0);

    if (clientSocket->lastError() == EWOULDBLOCK) {
        // we're likely dealing with an old version of CE that doesn't send the handle,
        // so grab the (presumably) only handle we have
        LOG(ERROR) << "Failed to receive handle. Assuming older version of CE (<7.4.1)";
        handle = state_->getProcessHandleLegacyCE();
    }

    if (!state_->containsProcessIDHandle(handle)) {
        LOG(ERROR) << "Handle " << handle << " has not opened any processes";
        return false;
    }

    VLOG(1) << "Getting memory model";
    auto memoryModel = memory->getMemoryModel(state_->getProcessID(handle));

    VLOG(1) << "Deducing architecture from memory model";
    auto arch = getArchitectureFromMemoryModel(memoryModel);

    VLOG(1) << "Done - detected arch as " << MemoryModelToString[memoryModel];

    send<unsigned char>(arch);

    return true;
}

unsigned char Client::getArchitectureFromMemoryModel(MemoryModel model) const {
    unsigned char result;
    switch (model) {
        case MemoryModel::MemoryModel_X86:
        case MemoryModel::MemoryModel_X86PAE:
            // x86
            result = 0;
            break;
        case MemoryModel::MemoryModel_X64:
            // x64 (AMD or Intel)
            result = 1;
            break;
        case MemoryModel::MemoryModel_ARM64:
            // ARM64
            result = 3;
            break;
        case MemoryModel::MemoryModel_UNKNOWN:
        case MemoryModel::MemoryModel_NA:
        default:
            LOG(ERROR) << "Unsupported architecture";
            result = -1; // will underflow to 0XFF
            break;
    }

    return result;
}

void Client::handleCommandGetSymbolListFromFile() {
    //auto fileOffset = read<int32_t>();
    auto symbolPathSize = read<int32_t>();
    char *symbolPath = new char[symbolPathSize];
    read(symbolPath, symbolPathSize);

    LOG(ERROR) << "Received request to get symbol list from file " << symbolPath << " but it's not yet supported";

    delete symbolPath;

    send<uint64_t>(0);
}

bool Client::handleCommandGetRegionInfo(unsigned char command) {
    auto handle = read<CE_HANDLE>();
    auto address = read<uint64_t>();

    if (!state_->containsProcessIDHandle(handle)) {
        LOG(ERROR) << "Handle " << handle << " has not opened any processes";
        return false;
    }

    if (address <= 0) {
        LOG(ERROR) << "Invalid address requested: " << std::hex << address;
        sendVirtualQueryExResult(nullptr);
        return true;
    }

    std::vector<MemoryRegion> regions = memory->getModuleSnapshot(state_->getProcessID(handle));
    MemoryRegion *respectiveMemoryRegion = nullptr;
    for (auto &region: regions) {
        if (region.vadEntry.vaStart <= address && address <= region.vadEntry.vaEnd) {
            respectiveMemoryRegion = &region;
            break;
        }
    }

    if (respectiveMemoryRegion == nullptr) {
        LOG(ERROR) << "Failed to find respective memory region for address " << std::hex << address;
    } else {
        VLOG(1) << "Found respective memory region for " << std::hex << address << ": "
            << std::hex << respectiveMemoryRegion->vadEntry.vaStart << " -> "
            << std::hex << respectiveMemoryRegion->vadEntry.vaEnd;
    }
    sendVirtualQueryExResult(respectiveMemoryRegion);

    if (command == CMD_GETREGIONINFO) {
        std::string truncated = "";
        if (respectiveMemoryRegion != nullptr) {
            truncated = respectiveMemoryRegion->name.substr(
                0, std::min((size_t) 127, respectiveMemoryRegion->name.size()));
        }

        send<BYTE>(truncated.size());
        if (truncated.size() > 0) {
            send(truncated.c_str(), truncated.size());
        }
    }

    return true;
}

bool Client::handleCommandVirtualQueryExFull() {
    auto handle = read<CE_HANDLE>();
    auto flags = read<uint8_t>();

    // unused for now
    int pagedOnly = flags & VQE_PAGEDONLY;
    int dirtyOnly = flags & VQE_DIRTYONLY;
    int noShared = flags & VQE_NOSHARED;

    if (!state_->containsProcessIDHandle(handle)) {
        LOG(ERROR) << "Handle " << handle << " has not opened any processes";
        return false;
    }

    std::vector<MemoryRegion> regions = memory->getModuleSnapshot(state_->getProcessID(handle));

    VLOG(2) << "Sending " << regions.size() << " memory regions";
    send<int32_t>(regions.size());
    for (auto &region: regions) {
        // here we'd filter based on the flags

        sendVirtualQueryExFullResult(&region);
    }

    return true;
}

std::string Client::vadProtectionToHumanReadable(PVMMDLL_MAP_VADENTRY vadEtry) const {
    // Copied nearly verbatim from the example at
    // https://github.com/ufrisk/MemProcFS/blob/master/vmm_example/vmmdll_example.c#L110-L121

    char sz[6];

    BYTE vh = (BYTE) vadEtry->Protection >> 3;
    BYTE vl = (BYTE) vadEtry->Protection & 7;
    sz[0] = vadEtry->fPrivateMemory ? 'p' : '-'; // PRIVATE MEMORY
    sz[1] = (vh & 2) ? ((vh & 1) ? 'm' : 'g') : ((vh & 1) ? 'n' : '-'); // -/NO_CACHE/GUARD/WRITECOMBINE
    sz[2] = ((vl == 1) || (vl == 3) || (vl == 4) || (vl == 6)) ? 'r' : '-'; // COPY ON WRITE
    sz[3] = (vl & 4) ? 'w' : '-'; // WRITE
    sz[4] = (vl & 2) ? 'x' : '-'; // EXECUTE
    sz[5] = ((vl == 5) || (vl == 7)) ? 'c' : '-'; // COPY ON WRITE
    if (sz[1] != '-' && sz[2] == '-' && sz[3] == '-' && sz[4] == '-' && sz[5] == '-') { sz[1] = '-'; }

    return std::string(sz);
}

int32_t Client::vadProtectionToWin32Protection(PVMMDLL_MAP_VADENTRY vadEntry) const {
    // not the most efficient way, but easier to grok
    std::string humanReadable = vadProtectionToHumanReadable(vadEntry);

    // int32_t protection = 0;

    // CE doesn't care about these flags
    /*
    if (humanReadable.at(1) == 'm') protection |= PAGE_WRITECOMBINE;
    if (humanReadable.at(1) == 'g') protection |= PAGE_GUARD;
    if (humanReadable.at(1) == 'n') protection |= PAGE_NOCACHE;
    if (humanReadable.at(5) == 'c') protection |= PAGE_WRITECOPY;
    */

    /*
    auto rwx = humanReadable.substr(2, 3);
    if      (rwx == "rwx") protection |= PAGE_EXECUTE_READWRITE;
    else if (rwx == "rw-") protection |= PAGE_READWRITE;
    else if (rwx == "r--") protection |= PAGE_READONLY;
    else if (rwx == "r-x") protection |= PAGE_EXECUTE_READ;
    else if (rwx == "--x") protection |= PAGE_EXECUTE;
    else if (rwx == "-wx") protection |= PAGE_EXECUTE_READWRITE;
    else if (rwx == "-w-") protection |= PAGE_READWRITE;
    else if (rwx == "---") protection |= PAGE_NOACCESS;
    else if (rwx == "w--") protection |= PAGE_WRITECOPY;
    else if (rwx == "wx-") protection |= PAGE_WRITECOPY;

    return protection;
    */

    if (humanReadable.at(4) == 'x') {
        // executable
        return humanReadable.at(3) == 'w' ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    }

    // non-executable
    return humanReadable.at(3) == 'w' ? PAGE_READWRITE : PAGE_READONLY;
}

int32_t Client::vadTypeToWin32Type(PVMMDLL_MAP_VADENTRY vadEntry) const {
    int32_t type;

    if (vadEntry->fPrivateMemory) return MEM_PRIVATE;

    switch (vadEntry->VadType) {
        case 2:
            type = MEM_IMAGE;
            break;
        case 1:
        default:
            type = MEM_MAPPED;
            break;
    }

    return type;
}

void Client::sendVirtualQueryExResult(MemoryRegion *region) const {
    auto result = CeVirtualQueryExOutput{};
    std::string name = "";
    std::string protectionHumanReadable = "?";

    if (region == nullptr) {
        result.result = 0;
        result.protection = 0;
        result.type = 0;
        result.baseaddress = 0;
        result.size = 0;
    } else {
        auto protection = vadProtectionToWin32Protection(&region->vadEntry);
        auto type = vadTypeToWin32Type(&region->vadEntry);

        result.result = 1;
        result.protection = protection;
        result.type = type;
        result.baseaddress = region->startAddr;
        result.size = region->regionSize;

        name = region->name;
        protectionHumanReadable = vadProtectionToHumanReadable(&region->vadEntry);
    }

    VLOG(2) << "Sending virtual query ex result (" << (int) result.result << ")"
        << " named " << name
        << " baseaddress " << std::hex << result.baseaddress
        << " size " << std::hex << result.size
        << " type " << result.type
        << " protection " << result.protection
        << " (" << protectionHumanReadable << ")";

    send<CeVirtualQueryExOutput>(result);
}

void Client::sendVirtualQueryExFullResult(MemoryRegion *region) const {
    auto result = CeRegionInfo{};

    auto protection = vadProtectionToWin32Protection(&region->vadEntry);
    auto type = vadTypeToWin32Type(&region->vadEntry);

    result.protection = protection;
    result.type = type;
    result.baseaddress = region->startAddr;
    result.size = region->regionSize;

    VLOG(2) << "Sending virtual query ex full result"
        << " named " << region->name
        << " baseaddress " << std::hex << result.baseaddress
        << " size " << std::hex << result.size
        << " type " << result.type
        << " protection " << result.protection
        << " (" << vadProtectionToHumanReadable(&region->vadEntry) << ")";

    send<CeRegionInfo>(result);
}

void Client::handleCommandStartDebug() const {
    auto handle = read<CE_HANDLE>();
    send<CE_HANDLE>(handle);
}

void Client::handleCommandGetOptions() const {
    send<uint16_t>(0);
}
