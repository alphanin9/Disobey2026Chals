#include <ProcessNotifyCallback.hpp>
#include <Misc.hpp>

PVOID ObCallbackRegistrationHandle{};

void DriverUnload(_In_ PDRIVER_OBJECT aDriver)
{
    UNICODE_STRING SymLink = RTL_CONSTANT_STRING(L"\\??\\FlagAntiCheat");

    ObUnRegisterCallbacks(ObCallbackRegistrationHandle);

    if (!NT_SUCCESS(PsSetCreateProcessNotifyRoutineEx(ProcOnProcessCreated, TRUE)))
    {
        KWARN("Note: failed to remove process creation callback during unload!");
    }

    auto protectedProcessId = InterlockedExchange64((volatile LONG64*)(&process::ProtectedProcessId), 0ull);

    if (protectedProcessId)
    {
        // Just in case anyone gets any ideas about unloading driver...

        CLIENT_ID cid
        {
            .UniqueProcess = (HANDLE)(protectedProcessId), .UniqueThread = 0ull
        };
        
        OBJECT_ATTRIBUTES attributes{.Length = sizeof(OBJECT_ATTRIBUTES)};
    
        HANDLE procHandle{};

        auto status = ZwOpenProcess(&procHandle, 0x1u, &attributes, &cid);

        if (NT_SUCCESS(status))
        {
            status = ZwTerminateProcess(procHandle, (NTSTATUS)(0x1337u));
            if (!NT_SUCCESS(status))
            {
                KWARN("Failed to terminate process! Status: 0x%08x", status);
            }

            ZwClose(procHandle);
        }
        else
        {
            KWARN("Failed to open process at driver unload! Status: 0x%08x", status);
        }
    }

    IoDeleteSymbolicLink(&SymLink);
    IoDeleteDevice(aDriver->DeviceObject);

    InterlockedExchangePointer((volatile PVOID*)(&Context::DeviceObject), nullptr);
}

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT aDriver, _In_ PUNICODE_STRING aRegistryPath)
{
    UNREFERENCED_PARAMETER(aRegistryPath);

    aDriver->DriverUnload = DriverUnload;

    PDEVICE_OBJECT deviceObject{};

    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\FlagAntiCheat");

    auto Status = IoCreateDevice(aDriver, 0u, &deviceName, FILE_DEVICE_UNKNOWN, 0u, FALSE, &deviceObject);
    if (!NT_SUCCESS(Status))
    {
        KWARN("IoCreateDevice failed: 0x%08X\n", Status);
        return Status;
    }

    Context::DeviceObject = deviceObject;

    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\FlagAntiCheat");

    Status = IoCreateSymbolicLink(&symLink, &deviceName);

    if (!NT_SUCCESS(Status))
    {
        KWARN("IoCreateSymbolicLink failed: 0x%08X\n", Status);
        IoDeleteDevice(deviceObject);
        InterlockedExchangePointer((volatile PVOID*)(&Context::DeviceObject), nullptr);
        return Status;
    }

    // Setup process notify callback...
    process::ProtectedProcessId = 0ull;
    Status = PsSetCreateProcessNotifyRoutineEx(ProcOnProcessCreated, FALSE);
    if (!NT_SUCCESS(Status))
    {
        KWARN("PsSetCreateProcessNotifyRoutineEx failed: 0x%08X\n", Status);

        IoDeleteSymbolicLink(&symLink);
        IoDeleteDevice(aDriver->DeviceObject);
        InterlockedExchangePointer((volatile PVOID*)(&Context::DeviceObject), nullptr);

        return Status;
    }

    process::ObjectCallbackOperations[0].ObjectType = PsProcessType;
    process::ObjectCallbackOperations[0].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
    process::ObjectCallbackOperations[0].PreOperation = ProcOnProcessHandlePreOpen;
    process::ObjectCallbackOperations[0].PostOperation = nullptr;

    process::ObjectCallbackOperations[1].ObjectType = PsThreadType;
    process::ObjectCallbackOperations[1].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
    process::ObjectCallbackOperations[1].PreOperation = ProcOnThreadHandlePreOpen;
    process::ObjectCallbackOperations[1].PostOperation = nullptr;

    process::ObjectCallbackRegistration.Version = OB_FLT_REGISTRATION_VERSION;
    process::ObjectCallbackRegistration.OperationRegistrationCount = 2u;
    process::ObjectCallbackRegistration.Altitude = RTL_CONSTANT_STRING(L"326969");
    process::ObjectCallbackRegistration.RegistrationContext = nullptr;
    process::ObjectCallbackRegistration.OperationRegistration = process::ObjectCallbackOperations;
    
    Status = ObRegisterCallbacks(&process::ObjectCallbackRegistration, &ObCallbackRegistrationHandle);

    if (!NT_SUCCESS(Status))
    {
        KWARN("ObRegisterCallbacks failed: 0x%08X\n", Status);

        PsSetCreateProcessNotifyRoutineEx(ProcOnProcessCreated, TRUE);
        IoDeleteSymbolicLink(&symLink);
        IoDeleteDevice(aDriver->DeviceObject);
        InterlockedExchangePointer((volatile PVOID*)(&Context::DeviceObject), nullptr);

        return Status;
    }

    return STATUS_SUCCESS;
}