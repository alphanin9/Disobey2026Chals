#include <ntddk.h>

#include <Context.hpp>
#include <DispatchHandlers.hpp>

extern "C" NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT aDriver,
    _In_ PUNICODE_STRING aRegistryPath
)
{
    UNREFERENCED_PARAMETER(aRegistryPath);

    aDriver->DriverUnload = [](PDRIVER_OBJECT aDriver) {
        UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\1337RGBController");

        IoDeleteSymbolicLink(&symLink);
        IoDeleteDevice(aDriver->DeviceObject);
    };
	aDriver->MajorFunction[IRP_MJ_CREATE] = DispatchHandlers::OnCreateAndClose;
    aDriver->MajorFunction[IRP_MJ_CLOSE] = DispatchHandlers::OnCreateAndClose;
	aDriver->MajorFunction[IRP_MJ_CLEANUP] = DispatchHandlers::OnCleanup;
	aDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchHandlers::OnIoControl;

    PDEVICE_OBJECT DeviceObject{};

	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\1337RGBController");

    auto Status = IoCreateDevice(aDriver, 0u, &DeviceName, FILE_DEVICE_UNKNOWN, 0u, FALSE, &DeviceObject);

    if (!NT_SUCCESS(Status)) {
		KWARN("IoCreateDevice failed: 0x%08X\n", Status);
        return Status;
    }

	UNICODE_STRING SymLink = RTL_CONSTANT_STRING(L"\\??\\1337RGBController");

	Status = IoCreateSymbolicLink(&SymLink, &DeviceName);

    if (!NT_SUCCESS(Status)) {
        KWARN("IoCreateSymbolicLink failed: 0x%08X\n", Status);
		IoDeleteDevice(DeviceObject);
        return Status;
    }

	ExInitializeFastMutex(&Context::ProcessListMutex);
    
    return STATUS_SUCCESS;
}
