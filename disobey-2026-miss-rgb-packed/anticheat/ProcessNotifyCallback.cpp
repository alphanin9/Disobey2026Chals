#include <ntifs.h>
// These go below because otherwise compiler gets mad
#include <Misc.hpp>
#include <ProcessNotifyCallback.hpp>

// Just in case so nobody gets any weird ideas...
// DISOBEY[w3_w1ll_add_cr3_shuffl1ng_t0_our_ant1ch3at_n3xt]
const UINT8 Flag[] = {67,  120, 245, 212, 44,  23,  221, 93, 78,  203, 152, 164, 17,  212, 228, 32,  137, 80, 57,
                      193, 107, 182, 77,  241, 130, 173, 35, 251, 111, 150, 159, 2,   140, 38,  13,  199, 29, 93,
                      241, 152, 247, 126, 39,  210, 106, 6,  125, 184, 33,  120, 112, 233, 53,  236, 65,  84, 237};

const UINT8 FlagXorKey[] = {7, 49, 166, 155, 110, 82, 132, 6, 57, 248, 199, 211, 32, 184, 136, 127, 232, 52, 93, 158, 8, 196, 126, 174, 241, 197, 86, 157, 9, 250, 174, 108, 235, 121, 121, 247, 66, 50, 132, 234, 168, 31, 73, 166, 91, 101, 21, 139, 64, 12, 47, 135, 6, 148, 53, 9, 237};

extern "C" __declspec(dllimport) LPCSTR PsGetProcessImageFileName(PEPROCESS);
extern "C" __declspec(dllimport) PVOID PsGetProcessSectionBaseAddress(PEPROCESS);

void process::OnProcessCreated(_Inout_ PEPROCESS aProcess, _In_ HANDLE aProcessId,
                               _Inout_opt_ PPS_CREATE_NOTIFY_INFO aCreateInfo)
{
    UINT32 processId = (UINT32)(aProcessId);
    if (!aCreateInfo)
    {
        // Process is exiting, is it ours?
        if (processId == ProtectedProcessId)
        {
            KTRACE("Process %u is exiting and is thus no longer protected.", processId);
            InterlockedExchange64((volatile LONG64*)(&ProtectedProcessId), 0ull);
        }

        return;
    }

    if (aCreateInfo->IsSubsystemProcess)
    {
        // 100% not ours, this is for WSL1 and things like that
        return;
    }

    aCreateInfo->CreationStatus = STATUS_SUCCESS;

    UNICODE_STRING ProtectedProcessNameStr = RTL_CONSTANT_STRING(L"*ChallengeBinary.exe");

    BOOLEAN shouldProtect = FALSE;

    __try
    {
        // This throws for some reason?
        shouldProtect =
            FsRtlIsNameInExpression(&ProtectedProcessNameStr, &aCreateInfo->FileObject->FileName, FALSE, nullptr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return;
    }

    if (!shouldProtect)
    {
        return;
    }

    if (InterlockedCompareExchange64((volatile LONG64*)(&ProtectedProcessId), (UINT64)(processId), 0ull))
    {
        // We are protecting another process??? Do not let this one execute.
        KTRACE("Another protected process is already executing! Killing launching one.");
        aCreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
        return;
    }

    // Check if behavior repeats if we don't write to process object at all...

    auto workItem = IoAllocateWorkItem(Context::DeviceObject);

    if (!workItem)
    {
        KWARN("Failed to create work item due to insufficient resources!");
        InterlockedExchange64((volatile LONG64*)(&ProtectedProcessId), 0ull);
        aCreateInfo->CreationStatus = STATUS_INSUFFICIENT_RESOURCES;
        return;
    }

    // Evil hack of evil hacks
    IoQueueWorkItem(
        workItem,
        [](PDEVICE_OBJECT, PVOID aContext)
        {
            auto workItem = (PIO_WORKITEM)(aContext);
            IoFreeWorkItem(workItem);

            // Hopefully no lock needed :)
            const auto protectedProcessId = ProtectedProcessId;

            PEPROCESS process{};

            // Note: unlikely scenario, but what if process dies somewhere in initialization?
            if (!NT_SUCCESS(PsLookupProcessByProcessId((HANDLE)(protectedProcessId), &process)))
            {
                KWARN("Failed to get protected process by ID!");
                return;
            }

            CHAR decodedFlag[sizeof(Flag)]{};

            RtlCopyMemory(decodedFlag, Flag, sizeof(Flag));

            for (auto i = 0u; i < sizeof(Flag); i++)
            {
                decodedFlag[i] ^= FlagXorKey[i];
            }

            PVOID sectionBaseAddress = PsGetProcessSectionBaseAddress(process);

            KAPC_STATE backupApcState{};
            KeStackAttachProcess(process, &backupApcState);

            // Fuck it, SEH the whole context attach lol
            __try
            {
                // Hack, OSR don't kill me plz...
                KTRACE("Image base address: %p", sectionBaseAddress);

                auto flagAddress = (PCHAR)(sectionBaseAddress) + 0x19000ull;

                RtlCopyMemory(flagAddress, decodedFlag, sizeof(decodedFlag));

                KTRACE("Flag write to process virtual address space complete.");
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                KWARN("Something segfaulted during copying flag to userspace!");
            }

            KeUnstackDetachProcess(&backupApcState);

            ObDereferenceObject(process);
        },
        DelayedWorkQueue, workItem);

    KTRACE("Process %u is protected. Waiting for write to virtual address space.", processId);
}

/**
 * \brief Ascertains whether or not this process should be able to open ours.
 *
 * \param aProcess The process.
 * \return Whether or not the process should get full privileges.
 */
NTSTATUS IsProcessLegitimateForHandleOpening(PEPROCESS aProcess)
{
    // I hope nobody names their attacker process that...
    constexpr const STRING WhitelistedNames[] = {
        RTL_CONSTANT_STRING("svchost.exe"), RTL_CONSTANT_STRING("explorer.exe"), RTL_CONSTANT_STRING("csrss.exe"),
        RTL_CONSTANT_STRING("lsass.exe"),   RTL_CONSTANT_STRING("WerFault.exe"), RTL_CONSTANT_STRING("MsMpEng.exe")};
    constexpr auto WhitelistedNameCount = ARRAYSIZE(WhitelistedNames);

    LPCSTR imageName = PsGetProcessImageFileName(aProcess);

    if (!imageName)
    {
        return STATUS_NOT_FOUND;
    }

    STRING imageNameStr{};

    RtlInitString(&imageNameStr, imageName);

    for (auto i = 0u; i < WhitelistedNameCount; i++)
    {
        if (RtlCompareString(&WhitelistedNames[i], &imageNameStr, false) == 0u)
        {
            return STATUS_SUCCESS;
        }
    }

    return STATUS_ACCESS_DENIED;
}

OB_PREOP_CALLBACK_STATUS process::OnProcessHandlePreOpen(_In_ PVOID,
                                                         _In_ POB_PRE_OPERATION_INFORMATION aOperationInformation)
{
    if (!ProtectedProcessId)
    {
        return OB_PREOP_SUCCESS;
    }

    auto target = (PEPROCESS)(aOperationInformation->Object);
    auto pid = (UINT64)(PsGetProcessId(target));

    // I think this should be fine lockless?
    if (pid != ProtectedProcessId)
    {
        return OB_PREOP_SUCCESS;
    }

    auto currentPid = PsGetCurrentProcessId();

    if (currentPid == (HANDLE)(pid))
    {
        // Blanket approve all local handles (why would you open a handle to yourself I don't get, but OK :P)
        return OB_PREOP_SUCCESS;
    }

    if (NT_SUCCESS(IsProcessLegitimateForHandleOpening(PsGetCurrentProcess())))
    {
        return OB_PREOP_SUCCESS;
    }

    constexpr auto PROCESS_TERMINATE = 0x1u;
    constexpr auto PROCESS_SYNCHRONIZE = SYNCHRONIZE;

    constexpr auto MaximumMaskForBasicProcesses = PROCESS_TERMINATE | PROCESS_SYNCHRONIZE;

    auto originalWantedAccess = 0u;
    auto gotAccess = 0u;
    switch (aOperationInformation->Operation)
    {
    case OB_OPERATION_HANDLE_CREATE:
    {
        aOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= MaximumMaskForBasicProcesses;
        originalWantedAccess = aOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess;
        gotAccess = aOperationInformation->Parameters->CreateHandleInformation.DesiredAccess;
        break;
    }
    case OB_OPERATION_HANDLE_DUPLICATE:
    {
        aOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= MaximumMaskForBasicProcesses;
        originalWantedAccess = aOperationInformation->Parameters->DuplicateHandleInformation.OriginalDesiredAccess;
        gotAccess = aOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess;
        break;
    }
    default:
    {
        break;
    }
    }

    KTRACE("Process ID %d is trying to open handle to protected process with desired rights %08x, received %08x",
           (int)(currentPid), originalWantedAccess, gotAccess);
    return OB_PREOP_SUCCESS;
}

OB_PREOP_CALLBACK_STATUS process::OnThreadHandlePreOpen(_In_ PVOID,
                                                        _In_ POB_PRE_OPERATION_INFORMATION aOperationInformation)
{
    if (!ProtectedProcessId)
    {
        return OB_PREOP_SUCCESS;
    }

    auto target = (PETHREAD)(aOperationInformation->Object);
    auto ownerProcessId = (UINT64)(PsGetThreadProcessId(target));

    // I think this should be fine lockless?
    if (ownerProcessId != ProtectedProcessId)
    {
        return OB_PREOP_SUCCESS;
    }

    auto currentPid = PsGetCurrentProcessId();

    if (currentPid == (HANDLE)(ownerProcessId))
    {
        // Blanket approve all local handles (ok for thread handles it's legit I guess)
        return OB_PREOP_SUCCESS;
    }

    if (NT_SUCCESS(IsProcessLegitimateForHandleOpening(PsGetCurrentProcess())))
    {
        return OB_PREOP_SUCCESS;
    }

    // What if evil APC queuer forces us to print flag? The horror...
    constexpr auto MaximumMask = SYNCHRONIZE;

    auto originalWantedAccess = 0u;
    auto gotAccess = 0u;

    switch (aOperationInformation->Operation)
    {
    case OB_OPERATION_HANDLE_CREATE:
    {
        aOperationInformation->Parameters->CreateHandleInformation.DesiredAccess &= MaximumMask;
        originalWantedAccess = aOperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess;
        gotAccess = aOperationInformation->Parameters->CreateHandleInformation.DesiredAccess;
        break;
    }
    case OB_OPERATION_HANDLE_DUPLICATE:
    {
        aOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess &= MaximumMask;
        originalWantedAccess = aOperationInformation->Parameters->DuplicateHandleInformation.OriginalDesiredAccess;
        gotAccess = aOperationInformation->Parameters->DuplicateHandleInformation.DesiredAccess;
        break;
    }
    default:
    {
        break;
    }
    }

    KTRACE("Process ID %d is trying to open handle to protected process thread with desired rights %08x, received %08x",
           (int)(currentPid), originalWantedAccess, gotAccess);

    return OB_PREOP_SUCCESS;
}

extern "C" void ProcOnProcessCreated(_Inout_ PEPROCESS aProcess, _In_ HANDLE aProcessId,
                                     _Inout_opt_ PPS_CREATE_NOTIFY_INFO aCreateInfo)
{
    return process::OnProcessCreated(aProcess, aProcessId, aCreateInfo);
}

extern "C" OB_PREOP_CALLBACK_STATUS ProcOnProcessHandlePreOpen(_In_ PVOID aRegistrationContext,
                                                               _In_ POB_PRE_OPERATION_INFORMATION aOperationInformation)
{
    return process::OnProcessHandlePreOpen(aRegistrationContext, aOperationInformation);
}

extern "C" OB_PREOP_CALLBACK_STATUS ProcOnThreadHandlePreOpen(_In_ PVOID aRegistrationContext,
                                                              _In_ POB_PRE_OPERATION_INFORMATION aOperationInformation)
{
    return process::OnThreadHandlePreOpen(aRegistrationContext, aOperationInformation);
}
