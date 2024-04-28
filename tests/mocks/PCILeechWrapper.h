#pragma once

#include "../../PCILeechWrapper.h"
#include <gtest/gtest.h>

class MockPCILeechWrapper : public PCILeechWrapper {
public:
    MOCK_METHOD(void, VMMDLL_Close, ( VMM_HANDLE), (const, override));

    MOCK_METHOD(VMM_HANDLE, VMMDLL_Initialize, ( DWORD, LPCSTR*), (const, override));

    MOCK_METHOD(bool, VMMDLL_Map_GetPhysMem, ( VMM_HANDLE , PVMMDLL_MAP_PHYSMEM* ), (const, override));

    MOCK_METHOD(bool, VMMDLL_PidList, ( VMM_HANDLE , PDWORD , PSIZE_T), (const, override));

    MOCK_METHOD(bool, VMMDLL_Map_GetVadU, ( VMM_HANDLE , DWORD , BOOL, PVMMDLL_MAP_VAD *), (const, override));

    MOCK_METHOD(bool, VMMDLL_MemReadEx, ( VMM_HANDLE , DWORD , ULONG64 , PBYTE , DWORD , PDWORD , ULONG64 ),
                (const, override));

    MOCK_METHOD(bool, VMMDLL_MemWrite, ( VMM_HANDLE , DWORD , ULONG64 , PBYTE, DWORD ), (const, override));

    MOCK_METHOD(LPSTR, VMMDLL_ProcessGetInformationString, ( VMM_HANDLE , DWORD , DWORD ), (const, override));

    MOCK_METHOD(void, VMMDLL_MemFree, ( PVOID ), (const, override));

    MOCK_METHOD(bool, VMMDLL_Map_GetModuleU, (VMM_HANDLE, DWORD, PVMMDLL_MAP_MODULE*, DWORD), (const, override));

    MOCK_METHOD(bool, VMMDLL_Map_GetThread, (VMM_HANDLE , DWORD , PVMMDLL_MAP_THREAD *), (const ,override));

    MOCK_METHOD(VMMDLL_SCATTER_HANDLE, VMMDLL_Scatter_Initialize, ( VMM_HANDLE, DWORD, DWORD), (const, override));

    MOCK_METHOD(bool, VMMDLL_Scatter_Prepare, ( VMMDLL_SCATTER_HANDLE, QWORD, DWORD), (const, override));

    MOCK_METHOD(bool, VMMDLL_Scatter_Execute, (VMMDLL_SCATTER_HANDLE), (const, override));

    MOCK_METHOD(bool, VMMDLL_Scatter_Read, ( VMMDLL_SCATTER_HANDLE, QWORD, DWORD, PBYTE, PDWORD), (const, override));

    MOCK_METHOD(bool, VMMDLL_Scatter_Clear, ( VMMDLL_SCATTER_HANDLE, DWORD, DWORD), (const, override));

    MOCK_METHOD(void, VMMDLL_Scatter_CloseHandle, (VMMDLL_SCATTER_HANDLE), (const, override));
};
