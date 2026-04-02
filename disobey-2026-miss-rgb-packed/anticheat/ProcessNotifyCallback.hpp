#pragma once

#include <ntddk.h>

namespace process
{
/**
 * \brief Process creation notify routine called by Windows for a created process.
 *
 * We use this to:
 *
 *   a) tag process ID as protected at earliest possible time
 *
 *   b) just in case, prevent dupe processes
 *
 * \param aProcess The new process object.
 * \param aProcessId The new process's process ID.
 * \param aCreateInfo The new process's creation information. If this is nullptr, the process is exiting.
 */
void OnProcessCreated(_Inout_ PEPROCESS aProcess, _In_ HANDLE aProcessId,
                      _Inout_opt_ PPS_CREATE_NOTIFY_INFO aCreateInfo);

/**
 * \brief Pre-operation callback routine for process handles. We check if it's the protected process and restrict some
 * handle rights.
 * \param aRegistrationContext Registration context. Always NULL.
 * \param aOperationInformation Operation information.
 */
OB_PREOP_CALLBACK_STATUS OnProcessHandlePreOpen(_In_ PVOID aRegistrationContext,
                                                _In_ POB_PRE_OPERATION_INFORMATION aOperationInformation);

/**
 * \brief Pre-operation callback routine for thread handles. We check if it's the protected process and restrict
 * some handle rights.
 * \param aRegistrationContext Registration context. Always NULL.
 * \param aOperationInformation Operation information.
 */
OB_PREOP_CALLBACK_STATUS OnThreadHandlePreOpen(_In_ PVOID aRegistrationContext,
                                               _In_ POB_PRE_OPERATION_INFORMATION aOperationInformation);

/**
 * \brief Protected process's PID. You should always use atomic operations with this one.
 */
inline UINT64 ProtectedProcessId{};

inline OB_CALLBACK_REGISTRATION ObjectCallbackRegistration{};
inline OB_OPERATION_REGISTRATION ObjectCallbackOperations[2]{};
} // namespace process

extern "C" void ProcOnProcessCreated(_Inout_ PEPROCESS aProcess, _In_ HANDLE aProcessId,
                                     _Inout_opt_ PPS_CREATE_NOTIFY_INFO aCreateInfo);

extern "C" OB_PREOP_CALLBACK_STATUS ProcOnProcessHandlePreOpen(
    _In_ PVOID aRegistrationContext, _In_ POB_PRE_OPERATION_INFORMATION aOperationInformation);

extern "C" OB_PREOP_CALLBACK_STATUS ProcOnThreadHandlePreOpen(_In_ PVOID aRegistrationContext,
                                                              _In_ POB_PRE_OPERATION_INFORMATION aOperationInformation);