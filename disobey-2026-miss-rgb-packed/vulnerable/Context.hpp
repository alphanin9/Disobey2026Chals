#pragma once
#include <ntddk.h>

namespace Context {
	struct ProcessDescription {
		UINT64 ProcessId{};
		bool HasMappedPhysicalMemory{};
		PVOID MappedPhysicalMemoryPML4Address{};
		UINT32 MappedPhysicalMemoryPML4Index{};
	};

	constexpr auto MAX_PROCESS_COUNT = 10u;

	inline FAST_MUTEX ProcessListMutex{};
	inline ProcessDescription Processes[MAX_PROCESS_COUNT]{};
}

#define KTRACE(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, __VA_ARGS__);
#define KWARN(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, __VA_ARGS__);