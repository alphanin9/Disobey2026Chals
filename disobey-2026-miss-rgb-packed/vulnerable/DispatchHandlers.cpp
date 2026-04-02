#include <Context.hpp>
#include <DispatchHandlers.hpp>
#include <intrin.h>

#include <wil/resource.h>

#include <ntimage.h>

extern "C" __declspec(dllimport) void* PsGetProcessSectionBaseAddress(PEPROCESS);

enum
{
    PDPTE_P = 1ull << 0,
    PDPTE_RW = 1ull << 1,
    PDPTE_US = 1ull << 2,
    PDPTE_PWT = 1ull << 3,
    PDPTE_PCD = 1ull << 4,
    PDPTE_A = 1ull << 5,
    PDPTE_D = 1ull << 6,
    PDPTE_PS = 1ull << 7, // must be set for 1GiB pages
    PDPTE_G = 1ull << 8,
    PDPTE_PAT = 1ull << 12,
    PDPTE_NX = 1ull << 63
};

#define ONE_GIB (1ull << 30)
#define ONE_GIB_ALIGN_MASK (ONE_GIB - 1)

/**
 * \brief GPT CODE
 *
 * Create a 1GiB huge-page PDPTE entry from a 1GiB-aligned physical address.
 *
 * - aPhysBase must be 1GiB-aligned (lower 30 bits zero).
 * - aFlags should include permissions/caching/NX bits you want (e.g., PDPTE_RW|PDPTE_NX),
 *   but this function will forcibly set PDPTE_P and PDPTE_PS, and will mask out bits that
 *   must be zero for a valid 1GiB PDPTE.
 *
 * Returns true on success; on success, *aOutEntry receives the PDPTE value.
 */
bool MakePDPTEFor1GHugePage(UINT64 aPhysBase, UINT64 aFlags, UINT64* aOutEntry)
{
    if (!aOutEntry)
    {
        return false;
    }

    // 1GiB alignment requirement.
    if (aPhysBase & ONE_GIB_ALIGN_MASK)
    {
        return false;
    }

    // Physical address field for 1GiB PDPTE uses bits [51:30] placed into entry [51:30].
    // Mask to 52-bit physical addresses (common on x86-64); adjust if you model more.
    UINT64 addr_field = aPhysBase & 0x000fffffc0000000ull;

    // Bits [29:13] are reserved and must be zero for 1GiB mappings.
    // Also ensure PS is set, and Present is set.
    UINT64 entry = 0;
    entry |= addr_field;
    entry |= (aFlags & (PDPTE_RW | PDPTE_US | PDPTE_PWT | PDPTE_PCD | PDPTE_G | PDPTE_PAT | PDPTE_NX |
                        // You typically do not set A/D manually; hardware may set them. If your project
                        // wants software-managed A/D, include them in flags and keep them here:
                        PDPTE_A | PDPTE_D));
    entry |= PDPTE_P | PDPTE_PS;

    *aOutEntry = entry;
    return true;
}

namespace access
{
bool CheckUserInput(PIRP aIrp, PIO_STACK_LOCATION aStackLocation)
{
    const _KUSER_SHARED_DATA* SharedData = (const _KUSER_SHARED_DATA*)(0xFFFFF78000000000);

    // "happy birthday trixter!" xor'ed with 0x67
    constexpr UINT8 XoredInput[] = {15, 6, 23, 23, 30, 71, 5, 14, 21, 19, 15, 3, 6, 30, 71, 19, 21, 14, 31, 19, 2, 21, 70, 103};

    constexpr auto MAGIC_INPUT_SIZE = 24u;

    // Check if input buffer is magic constant and magic size
    const auto inputSize = aStackLocation->Parameters.DeviceIoControl.InputBufferLength;

    if (inputSize != MAGIC_INPUT_SIZE)
    {
        return false;
    }

    UINT8 unxoredInput[sizeof(XoredInput)]{};

    for (auto i = 0u; i < sizeof(XoredInput); i++)
    {
        unxoredInput[i] = XoredInput[i] ^ 0x67;
    }

    LARGE_INTEGER fileTime{};

    fileTime.LowPart = SharedData->SystemTime.LowPart;
    fileTime.HighPart = SharedData->SystemTime.High1Time;
    fileTime.QuadPart &= ~(0xFFFull); // Zero out lower bits for some leeway

    // Alias to UINT64 now
    UINT64* aliasedInput = (UINT64*)(unxoredInput);

    constexpr auto SizeOfAliasedInput = sizeof(XoredInput) / sizeof(UINT64);

    for (auto i = 0u; i < SizeOfAliasedInput; i++)
    {
        aliasedInput[i] ^= fileTime.QuadPart;
    }

    return RtlCompareMemory(aIrp->AssociatedIrp.SystemBuffer, unxoredInput, sizeof(unxoredInput)) == sizeof(unxoredInput);
}

bool CheckImageSections()
{
    auto currentProcess = PsGetCurrentProcess();

    if (PsInitialSystemProcess == currentProcess)
    {
        return false;
    }

    auto imageBase = PsGetProcessSectionBaseAddress(currentProcess);

    if (!imageBase)
    {
        // WTF?
        return false;
    }

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)(imageBase);

    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }

    PIMAGE_NT_HEADERS64 ntHeaders = (PIMAGE_NT_HEADERS64)((UINT8*)(imageBase) + dosHeader->e_lfanew);

    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE)
    {
        return false;
    }

    auto sections = IMAGE_FIRST_SECTION(ntHeaders);

    for (auto i = 0u; i < ntHeaders->FileHeader.NumberOfSections; i++)
    {
        constexpr UINT64 MagicSectionName = 'mvm.';
        auto aliasedName = *(UINT64*)(sections[i].Name);

        auto& section = sections[i];

        if(MagicSectionName == aliasedName) {
            return true;
        }
    }

    return false;
}
} // namespace access

NTSTATUS CompleteIRP(NTSTATUS aStatus, PIRP aIrp, ULONG_PTR aInformation = 0u)
{
    aIrp->IoStatus.Status = aStatus;
    aIrp->IoStatus.Information = aInformation;
    IoCompleteRequest(aIrp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS
DispatchHandlers::OnCreateAndClose(_In_ PDEVICE_OBJECT aDeviceObject, _In_ PIRP aIrp)
{
    UNREFERENCED_PARAMETER(aDeviceObject);
    return CompleteIRP(STATUS_SUCCESS, aIrp);
}

NTSTATUS
DispatchHandlers::OnCleanup(_In_ PDEVICE_OBJECT aDeviceObject, _In_ PIRP aIrp)
{
    UNREFERENCED_PARAMETER(aDeviceObject);
    // Process is exiting or closing handle to us
    // Find it in bookkeeping, if it has physmem mapped - cleanup

    auto currentProcessId = PsGetCurrentProcessId();

    ExAcquireFastMutex(&Context::ProcessListMutex);

    for (auto i = 0u; i < Context::MAX_PROCESS_COUNT; i++)
    {
        auto& processDesc = Context::Processes[i];
        if (processDesc.ProcessId == (UINT64)(currentProcessId))
        {
            if (processDesc.HasMappedPhysicalMemory)
            {
                // Unmap physical memory from process's PML4
                auto dtbMapping = (ULONG_PTR*)(MmGetVirtualForPhysical(
                    PHYSICAL_ADDRESS{.QuadPart = (LONGLONG)(__readcr3() & (~0xFFFull))}));
                if (dtbMapping)
                {
                    dtbMapping[processDesc.MappedPhysicalMemoryPML4Index] = 0;
                    const auto cr4 = __readcr4();
                    __writecr4(cr4 ^ 0x80); // Flush TLB
                    __writecr4(cr4);
                }
                processDesc.HasMappedPhysicalMemory = false;

                if (processDesc.MappedPhysicalMemoryPML4Address)
                {
                    ExFreePoolWithTag(processDesc.MappedPhysicalMemoryPML4Address, 'pmem');
                    processDesc.MappedPhysicalMemoryPML4Address = nullptr;
                }

                processDesc.MappedPhysicalMemoryPML4Index = 0u;
            }
            processDesc.ProcessId = 0u; // Mark as free
            break;
        }
    }

    // Cuz VS whines at me if I have it in scope guard...
    ExReleaseFastMutex(&Context::ProcessListMutex);

    return CompleteIRP(STATUS_SUCCESS, aIrp);
}

NTSTATUS
DispatchHandlers::OnIoControl(_In_ PDEVICE_OBJECT aDeviceObject, _In_ PIRP aIrp)
{
    UNREFERENCED_PARAMETER(aDeviceObject);
    auto irpStackLocation = IoGetCurrentIrpStackLocation(aIrp);
    auto& deviceIoControlParams = irpStackLocation->Parameters.DeviceIoControl;

    auto status = STATUS_INVALID_DEVICE_REQUEST;
    auto writtenSize = 0u;

    if (deviceIoControlParams.IoControlCode == IoctlMapPhysicalMemoryIntoPml4)
    {
        if (!access::CheckUserInput(aIrp, irpStackLocation) || !access::CheckImageSections())
        {
            status = STATUS_ACCESS_DENIED;
            return CompleteIRP(status, aIrp, writtenSize);
        }

        PHYSICAL_ADDRESS directoryTableBase{.QuadPart = (LONGLONG)(__readcr3() & (~0xFFFull))};

        auto dtbMapping = (ULONG_PTR*)(MmGetVirtualForPhysical(directoryTableBase));

        if (!dtbMapping)
        {
            status = STATUS_INVALID_PARAMETER; // Strictly speaking not correct NTSTATUS but it's OK
            return CompleteIRP(status, aIrp, writtenSize);
        }

        auto firstFreePML4Entry = 0xFFFFFFFF;

        // Hack to start count a bit higher...
        // Stolen from https://reversing.info/posts/guardedregions/
        for (auto i = 8u; i < 256u; i++)
        {
            if (!dtbMapping[i])
            {
                // No marking by Windows at all
                firstFreePML4Entry = i;
                break;
            }
        }

        if (firstFreePML4Entry == 0xFFFFFFFF)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            return CompleteIRP(status, aIrp, writtenSize);
        }

        auto physMemRanges = MmGetPhysicalMemoryRanges();
        auto freePhysMemRangesOnExit = wil::scope_exit(
            [&]() -> void
            {
                if (physMemRanges)
                {
                    ExFreePoolWithTag(physMemRanges, 0);
                }
            });

        if (!physMemRanges)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            return CompleteIRP(status, aIrp, writtenSize);
        }

        constexpr UINT64 HugePageSize = 1024ull * 1024ull * 1024ull; // 1 GB
        // OK sure this is kinda hack but yeah
        PHYSICAL_ADDRESS highestPhysicalAddress{};

        for (auto i = 0u; physMemRanges[i].BaseAddress.QuadPart != 0 || physMemRanges[i].NumberOfBytes.QuadPart != 0;
             i++)
        {
            const auto rangeEnd = physMemRanges[i].BaseAddress.QuadPart + physMemRanges[i].NumberOfBytes.QuadPart;

            if (rangeEnd > highestPhysicalAddress.QuadPart)
            {
                highestPhysicalAddress.QuadPart = rangeEnd - 1;
            }
        }

        auto neededHugePages = highestPhysicalAddress.QuadPart / HugePageSize;
        auto remainingBytes = highestPhysicalAddress.QuadPart % HugePageSize;

        auto pdpt = (UINT64*)(ExAllocatePoolZero(NonPagedPoolNx, 0x1000u, 'pmem'));
        auto freePdptOnExit = wil::scope_exit(
            [&]() -> void
            {
                if (pdpt)
                {
                    ExFreePoolWithTag(pdpt, 'pmem');
                }
            });

        if (!pdpt)
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            return CompleteIRP(status, aIrp, writtenSize);
        }

        // Note: we assume here that we have enough space in PML4E for mapping (512GB is enough lol; this is CTF)
        for (auto i = 0u; i < neededHugePages; i++)
        {
            MakePDPTEFor1GHugePage((UINT64)(i)*HugePageSize, PDPTE_RW | PDPTE_NX | PDPTE_US, &pdpt[i]);
        }

        if (remainingBytes > 0u)
        {
            MakePDPTEFor1GHugePage(neededHugePages * HugePageSize, PDPTE_RW | PDPTE_NX | PDPTE_US,
                                   &pdpt[neededHugePages]);
        }

        if (deviceIoControlParams.OutputBufferLength >= sizeof(PhysicalMemoryMappingInformation))
        {
            if (aIrp->AssociatedIrp.SystemBuffer == nullptr)
            {
                status = STATUS_INVALID_PARAMETER;
                return CompleteIRP(status, aIrp, writtenSize);
            }

            ExAcquireFastMutex(&Context::ProcessListMutex);

            for (auto& processDesc : Context::Processes)
            {
                if (processDesc.ProcessId == (UINT64)(PsGetCurrentProcessId()))
                {
                    if (processDesc.HasMappedPhysicalMemory)
                    {
                        // Already has mapped physmem?
                        status = STATUS_DEVICE_BUSY;
                        ExReleaseFastMutex(&Context::ProcessListMutex);
                        return CompleteIRP(status, aIrp, writtenSize);
                    }
                    freePdptOnExit.release(); // We are using it now
                    processDesc.HasMappedPhysicalMemory = true;
                    processDesc.MappedPhysicalMemoryPML4Address = pdpt;
                    processDesc.MappedPhysicalMemoryPML4Index = firstFreePML4Entry;
                    break;
                }
                else if (processDesc.ProcessId == 0u)
                {
                    // Free slot
                    freePdptOnExit.release(); // We are using it now
                    processDesc.ProcessId = (UINT64)(PsGetCurrentProcessId());
                    processDesc.HasMappedPhysicalMemory = true;
                    processDesc.MappedPhysicalMemoryPML4Address = pdpt;
                    processDesc.MappedPhysicalMemoryPML4Index = firstFreePML4Entry;
                    break;
                }
            }

            ExReleaseFastMutex(&Context::ProcessListMutex);

            struct HardwarePte
            {
                union
                {
                    struct
                    {
                        ULONGLONG Valid : 1;
                        ULONGLONG Write : 1;
                        ULONGLONG Owner : 1;
                        ULONGLONG WriteThrough : 1;
                        ULONGLONG CacheDisable : 1;
                        ULONGLONG Accessed : 1;
                        ULONGLONG Dirty : 1;
                        ULONGLONG LargePage : 1;
                        ULONGLONG Global : 1;
                        ULONGLONG CopyOnWrite : 1;
                        ULONGLONG Prototype : 1;
                        ULONGLONG reserved0 : 1;
                        ULONGLONG PageFrameNumber : 40;
                        ULONGLONG SoftwareWsIndex : 11;
                        ULONGLONG NoExecute : 1;
                    };
                    UINT64 Flags{};
                };
            };

            HardwarePte pml4eEntry{};

            pml4eEntry.Valid = 1;
            pml4eEntry.Write = 1;
            pml4eEntry.Owner = 1;
            pml4eEntry.PageFrameNumber = MmGetPhysicalAddress(pdpt).QuadPart >> 12;

            dtbMapping[firstFreePML4Entry] = pml4eEntry.Flags;

            const auto cr4 = __readcr4();
            __writecr4(cr4 ^ 0x80); // Flush TLB
            __writecr4(cr4);

            auto mappingInfo = (PhysicalMemoryMappingInformation*)(aIrp->AssociatedIrp.SystemBuffer);

            mappingInfo->DirectoryTableBase = directoryTableBase.QuadPart;

            auto baseVirtualAddress = (UINT64)(firstFreePML4Entry) << 39ull; // PML4E index is bits 39-47

            // TODO: doesn't work when CR3 is higher than some value, IDK why
            mappingInfo->MappingBaseVirtAddr = baseVirtualAddress;
            mappingInfo->MappingEndVirtAddr = 'mvm';

            mappingInfo->Process = (UINT64)(PsGetCurrentProcess());

            writtenSize = sizeof(PhysicalMemoryMappingInformation);
            status = STATUS_SUCCESS;
        }
        else
        {
            status = STATUS_BUFFER_TOO_SMALL;
        }
    }

    return CompleteIRP(status, aIrp, writtenSize);
}
