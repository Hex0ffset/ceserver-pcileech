#pragma once

// As of Cheat Engine 7.5
// https://github.com/cheat-engine/cheat-engine/blob/master/Cheat%20Engine/ceserver/ceserver.h

#define CMD_GETVERSION					0
#define CMD_CLOSECONNECTION				1
#define CMD_TERMINATESERVER				2
#define CMD_OPENPROCESS					3
#define CMD_CREATETOOLHELP32SNAPSHOT	4
#define CMD_PROCESS32FIRST				5
#define CMD_PROCESS32NEXT				6
#define CMD_CLOSEHANDLE					7
#define CMD_VIRTUALQUERYEX				8
#define CMD_READPROCESSMEMORY			9
#define CMD_WRITEPROCESSMEMORY			10
#define CMD_STARTDEBUG					11
#define CMD_STOPDEBUG					12
#define CMD_WAITFORDEBUGEVENT			13
#define CMD_CONTINUEFROMDEBUGEVENT		14
#define CMD_SETBREAKPOINT				15
#define CMD_REMOVEBREAKPOINT			16
#define CMD_SUSPENDTHREAD				17
#define CMD_RESUMETHREAD				18
#define CMD_GETTHREADCONTEXT			19
#define CMD_SETTHREADCONTEXT			20
#define CMD_GETARCHITECTURE				21
#define CMD_MODULE32FIRST				22
#define CMD_MODULE32NEXT				23
#define CMD_GETSYMBOLLISTFROMFILE		24
#define CMD_LOADEXTENSION				25
#define CMD_ALLOC						26
#define CMD_FREE						27
#define CMD_CREATETHREAD				28
#define CMD_LOADMODULE					29
#define CMD_SPEEDHACK_SETSPEED			30
#define CMD_VIRTUALQUERYEXFULL			31
#define CMD_GETREGIONINFO				32
#define CMD_GETABI						33
#define CMD_SET_CONNECTION_NAME			34
#define CMD_CREATETOOLHELP32SNAPSHOTEX	35
#define CMD_CHANGEMEMORYPROTECTION		36
#define CMD_GETOPTIONS					37
#define CMD_GETOPTIONVALUE				38
#define CMD_SETOPTIONVALUE				39
#define CMD_PTRACE_MMAP					40
#define CMD_OPENNAMEDPIPE				41
#define CMD_PIPEREAD					42
#define CMD_PIPEWRITE					43
#define CMD_GETCESERVERPATH				44
#define CMD_ISANDROID					45
#define CMD_LOADMODULEEX				46
#define CMD_SETCURRENTPATH				47
#define CMD_GETCURRENTPATH				48
#define CMD_ENUMFILES					49
#define CMD_GETFILEPERMISSIONS			50
#define CMD_SETFILEPERMISSIONS			51
#define CMD_GETFILE						52
#define CMD_PUTFILE						53
#define CMD_CREATEDIR					54
#define CMD_DELETEFILE					55
#define CMD_AOBSCAN						200
#define CMD_COMMANDLIST2				255

#define VQE_PAGEDONLY 1
#define VQE_DIRTYONLY 2
#define VQE_NOSHARED 4

#pragma pack(1)
typedef struct {
    int version;
    unsigned char stringsize;
    //append the versionstring
} CeVersion, *PCeVersion;

typedef struct {
    int result;
    int pid;
    int processnamesize;
    //processname
} CeProcessEntry, *PCeProcessEntry;

typedef struct {
    int32_t result;
    int64_t modulebase;
    int32_t modulepart;
    int32_t modulesize;
    // upcoming field, not yet used in 7.5
    //uint32_t modulefileoffset;
    int32_t modulenamesize;
} CeModuleEntry, *PCeModuleEntry;

typedef struct {
    uint8_t result;
    uint32_t protection;
    uint32_t type;
    uint64_t baseaddress;
    uint64_t size;
} CeVirtualQueryExOutput, *PCeVirtualQueryExOutput;

typedef struct {
    uint32_t protection;
    uint32_t type;
    uint64_t baseaddress;
    uint64_t size;
} CeVirtualQueryExFullOutput, *PCeVirtualQueryExFullOutput;

typedef struct {
    uint64_t baseaddress;
    uint64_t size;
    uint32_t protection;
    uint32_t type;
} CeRegionInfo, *PCeRegionInfo;

#pragma pack()