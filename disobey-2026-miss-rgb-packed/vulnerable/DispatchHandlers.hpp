#pragma once
#include <ntddk.h>
#include <wdm.h>

namespace DispatchHandlers
{
NTSTATUS OnCreateAndClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

NTSTATUS OnCleanup(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);

NTSTATUS OnIoControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);
} // namespace DispatchHandlers

struct PhysicalMemoryMappingInformation
{
    UINT64 DirectoryTableBase{};  // 00 - gift
    UINT64 Process{};             // 08 - gift
    UINT64 MappingBaseVirtAddr{}; // 10
    UINT64 MappingEndVirtAddr{};  // 18 - set to MVM
};

inline constexpr auto IoctlMapPhysicalMemoryIntoPml4 =
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1337, METHOD_BUFFERED, FILE_ANY_ACCESS);
