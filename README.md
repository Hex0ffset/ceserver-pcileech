# CEServer PCILeech

[![CircleCI](https://dl.circleci.com/status-badge/img/circleci/Hwp5S7hYxryqBMiDgoN2vX/U3FKRrLtpYA2ttdiRZQAJp/tree/main.svg?style=svg)](https://dl.circleci.com/status-badge/redirect/circleci/Hwp5S7hYxryqBMiDgoN2vX/U3FKRrLtpYA2ttdiRZQAJp/tree/main)

A Cheat Engine server for DMA based memory access using PCILeech.

Tested with the following Cheat Engine versions:

- Cheat Engine 7.5
- Cheat Engine 7.4 (partially tested)

Tested on the following OS's:

- Windows 11
- RaspberryPi OS (Pi 5)

Currently supported commands:

| Command                        | Supported? |
|--------------------------------|------------|
| CMD_CREATETOOLHELP32SNAPSHOT   | ✅          |
| CMD_PROCESS32FIRST             | ✅          |
| CMD_PROCESS32NEXT              | ✅          |
| CMD_MODULE32FIRST              | ✅          |
| CMD_MODULE32NEXT               | ✅          |
| CMD_CLOSEHANDLE                | ✅          |
| CMD_GETVERSION                 | ✅          |
| CMD_GETABI                     | ✅          |
| CMD_CREATETOOLHELP32SNAPSHOTEX | ✅          |
| CMD_SET_CONNECTION_NAME        | ✅          |
| CMD_CLOSECONNECTION            | ✅          |
| CMD_TERMINATESERVER            | ✅          |
| CMD_OPENPROCESS                | ✅          |
| CMD_READPROCESSMEMORY          | ✅          |
| CMD_WRITEPROCESSMEMORY         | ✅          |
| CMD_GETARCHITECTURE            | ✅          |
| CMD_GETSYMBOLLISTFROMFILE      | 🟧 partial |
| CMD_VIRTUALQUERYEX             | ✅          |
| CMD_GETREGIONINFO              | ✅          |
| CMD_VIRTUALQUERYEXFULL         | ✅          |
| CMD_STARTDEBUG                 | 🟧 partial |
| CMD_GETOPTIONS                 | 🟧 partial |
| CMD_STOPDEBUG                  | ❌          |
| CMD_WAITFORDEBUGEVENT          | ❌          |
| CMD_CONTINUEFROMDEBUGEVENT     | ❌          |
| CMD_SETBREAKPOINT              | ❌          |
| CMD_REMOVEBREAKPOINT           | ❌          |
| CMD_SUSPENDTHREAD              | ❌          |
| CMD_RESUMETHREAD               | ❌          |
| CMD_GETTHREADCONTEXT           | ❌          |
| CMD_SETTHREADCONTEXT           | ❌          |
| CMD_LOADEXTENSION              | ❌          |
| CMD_ALLOC                      | ❌          |
| CMD_FREE                       | ❌          |
| CMD_CREATETHREAD               | ❌          |
| CMD_LOADMODULE                 | ❌          |
| CMD_SPEEDHACK_SETSPEED         | ❌          |
| CMD_CHANGEMEMORYPROTECTION     | ❌          |
| CMD_GETOPTIONVALUE             | ❌          |
| CMD_SETOPTIONVALUE             | ❌          |
| CMD_PTRACE_MMAP                | ❌          |
| CMD_OPENNAMEDPIPE              | ❌          |
| CMD_PIPEREAD                   | ❌          |
| CMD_PIPEWRITE                  | ❌          |
| CMD_GETCESERVERPATH            | ❌          |
| CMD_ISANDROID                  | ❌          |
| CMD_LOADMODULEEX               | ❌          |
| CMD_SETCURRENTPATH             | ❌          |
| CMD_GETCURRENTPATH             | ❌          |
| CMD_ENUMFILES                  | ❌          |
| CMD_GETFILEPERMISSIONS         | ❌          |
| CMD_SETFILEPERMISSIONS         | ❌          |
| CMD_GETFILE                    | ❌          |
| CMD_PUTFILE                    | ❌          |
| CMD_CREATEDIR                  | ❌          |
| CMD_DELETEFILE                 | ❌          |
| CMD_AOBSCAN                    | ❌          |
| CMD_COMMANDLIST2               | ❌          |

## Building / Installing

### Windows

Install build dependencies

* Visual Studio 2022
* Chocolatey

```powershell
choco install cmake -y
```

Install ceserver-pcileech

```powershell
mkdir build
cd build
cmake ..
cmake ../ -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022" -Ax64 -DCMAKE_GENERATOR_PLATFORM=x64 -DCMAKE_INSTALL_PREFIX=c:/ceserver-pcileech --target install
```

Run ceserver-pcileech

```powersehll
& "C:/ceserver-pcileech/bin/ceserver-pcileech.exe"
```

### Linux

Install PCILeech dependencies

```bash
sudo apt install libusb-1.0-0-dev libfuse-dev liblz4-dev
```

Install ceserver-pcileech

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release --target install
```

Run ceserver-pcileech

```bash
ceserver-pcileech
```