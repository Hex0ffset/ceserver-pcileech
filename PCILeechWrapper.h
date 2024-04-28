#pragma once

#ifdef _WIN32
#ifdef WIN32_LEAN_AND_MEAN
#define UMDF_USING_NTSTATUS
#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#include <d3dkmthk.h>
#endif
#endif
#include <vmmdll.h>

class PCILeechWrapper {
public:
    PCILeechWrapper() {
    };

    virtual void VMMDLL_Close(_In_opt_ _Post_ptr_invalid_ VMM_HANDLE hVMM) const {
        ::VMMDLL_Close(hVMM);
    }

    virtual VMM_HANDLE VMMDLL_Initialize(_In_ DWORD argc, _In_ LPCSTR argv[]) const {
        return ::VMMDLL_Initialize(argc, argv);
    }

    virtual bool VMMDLL_Map_GetPhysMem(_In_ VMM_HANDLE hVMM, _Out_ PVMMDLL_MAP_PHYSMEM *ppPhysMemMap) const {
        return ::VMMDLL_Map_GetPhysMem(hVMM, ppPhysMemMap);
    }

    virtual bool VMMDLL_PidList(_In_ VMM_HANDLE hVMM, _Out_writes_opt_(*pcPIDs) PDWORD pPIDs,
                                _Inout_ PSIZE_T pcPIDs) const {
        return ::VMMDLL_PidList(hVMM, pPIDs, pcPIDs);
    }

    virtual bool VMMDLL_Map_GetVadU(_In_ VMM_HANDLE hVMM, _In_ DWORD dwPID, _In_ BOOL fIdentifyModules,
                                    _Out_ PVMMDLL_MAP_VAD *ppVadMap) const {
        return ::VMMDLL_Map_GetVadU(hVMM, dwPID, fIdentifyModules, ppVadMap);
    }

    virtual bool VMMDLL_MemReadEx(_In_ VMM_HANDLE hVMM, _In_ DWORD dwPID, _In_ ULONG64 qwA, _Out_writes_(cb) PBYTE pb,
                                  _In_ DWORD cb, _Out_opt_ PDWORD pcbReadOpt, _In_ ULONG64 flags) const {
        return ::VMMDLL_MemReadEx(hVMM, dwPID, qwA, pb, cb, pcbReadOpt, flags);
    }

    virtual bool VMMDLL_MemWrite(_In_ VMM_HANDLE hVMM, _In_ DWORD dwPID, _In_ ULONG64 qwA, _In_reads_(cb) PBYTE pb,
                                 _In_ DWORD cb) const {
        return ::VMMDLL_MemWrite(hVMM, dwPID, qwA, pb, cb);
    }

    virtual LPSTR VMMDLL_ProcessGetInformationString(_In_ VMM_HANDLE hVMM, _In_ DWORD dwPID,
                                                     _In_ DWORD fOptionString) const {
        return ::VMMDLL_ProcessGetInformationString(hVMM, dwPID, fOptionString);
    }

    virtual void VMMDLL_MemFree(_Frees_ptr_opt_ PVOID pvMem) const {
        ::VMMDLL_MemFree(pvMem);
    }

    virtual bool VMMDLL_Map_GetModuleU(_In_ VMM_HANDLE hVMM, _In_ DWORD dwPID, _Out_ PVMMDLL_MAP_MODULE *ppModuleMap,
                                       _In_ DWORD flags) const {
        return ::VMMDLL_Map_GetModuleU(hVMM, dwPID, ppModuleMap, flags);
    }

    virtual bool VMMDLL_Map_GetThread(_In_ VMM_HANDLE hVMM, _In_ DWORD dwPID,
                                      _Out_ PVMMDLL_MAP_THREAD *ppThreadMap) const {
        return ::VMMDLL_Map_GetThread(hVMM, dwPID, ppThreadMap);
    }

    virtual bool VMMDLL_ProcessGetInformation(_In_ VMM_HANDLE hVMM,_In_ DWORD dwPID,
                                              _Inout_opt_ PVMMDLL_PROCESS_INFORMATION pProcessInformation,
                                              _In_ PSIZE_T pcbProcessInformation) const {
        return ::VMMDLL_ProcessGetInformation(hVMM, dwPID, pProcessInformation, pcbProcessInformation);
    }

    virtual VMMDLL_SCATTER_HANDLE VMMDLL_Scatter_Initialize(_In_ VMM_HANDLE hVMM, _In_ DWORD dwPID, _In_ DWORD flags) const {
        return ::VMMDLL_Scatter_Initialize(hVMM, dwPID, flags);
    }

    virtual bool VMMDLL_Scatter_Prepare(_In_ VMMDLL_SCATTER_HANDLE hS, _In_ QWORD va, _In_ DWORD cb) const {
        return ::VMMDLL_Scatter_Prepare(hS, va, cb);
    }

    virtual bool VMMDLL_Scatter_Execute(_In_ VMMDLL_SCATTER_HANDLE hS) const {
        return ::VMMDLL_Scatter_Execute(hS);
    }

    virtual bool VMMDLL_Scatter_Read(_In_ VMMDLL_SCATTER_HANDLE hS, _In_ QWORD va, _In_ DWORD cb, _Out_writes_opt_(cb) PBYTE pb, _Out_opt_ PDWORD pcbRead) const {
        return ::VMMDLL_Scatter_Read(hS, va, cb, pb, pcbRead);
    }

    virtual bool VMMDLL_Scatter_Clear(_In_ VMMDLL_SCATTER_HANDLE hS, _In_opt_ DWORD dwPID, _In_ DWORD flags) const {
        return ::VMMDLL_Scatter_Clear(hS, dwPID, flags);
    }

    virtual void VMMDLL_Scatter_CloseHandle(_In_opt_ _Post_ptr_invalid_ VMMDLL_SCATTER_HANDLE hS) const {
        ::VMMDLL_Scatter_CloseHandle(hS);
    }
};
