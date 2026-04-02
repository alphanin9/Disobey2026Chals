#pragma once

#include <ntddk.h>

#define KTRACE(FormatString, ...) DbgPrint("[FlagAntiCheat Trace] " FormatString, __VA_ARGS__)
#define KWARN(FormatString, ...) DbgPrint("[FlagAntiCheat Warn] " FormatString, __VA_ARGS__)

namespace Context
{
inline PDEVICE_OBJECT DeviceObject{};
}
