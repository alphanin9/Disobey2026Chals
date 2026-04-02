#include <Windows.h>
#include <cstdint>
#include <winnt.h>

#include <intrin.h>

#include <cctype>
#include <cmath>
#include <string_view>

#include <Shim.hpp>

constexpr auto DebugKey = false;
extern "C" void* __ImageBase;

#pragma comment(linker, "/SECTION:INIT,ER")
#pragma comment(linker, "/SECTION:MVM,RWE")

#pragma code_seg(push, "INIT")
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        // Decrypt MVM section here
        // Kinda obvious but yeah what can you do lol
        auto dosHeaders = (PIMAGE_DOS_HEADER)(&__ImageBase);
        auto ntHeaders = (PIMAGE_NT_HEADERS)((char*)(&__ImageBase) + dosHeaders->e_lfanew);

        auto sections = IMAGE_FIRST_SECTION(ntHeaders);

        for (auto i = 0u; i < ntHeaders->FileHeader.NumberOfSections; i++)
        {
            auto section = sections[i];
            auto sectionNameAsInteger = (uint32_t*)(section.Name);

            if (*sectionNameAsInteger != 'MVM')
            {
                continue;
            }

            if (sectionNameAsInteger[1] == 0)
            {
                // We use this as tag that section is encrypted
                continue;
            }

            uint8_t nonce[16]{};
            for (auto& i : nonce)
            {
                i = 67;
            }

            DWORD key[4]{};
            key[0] = ~section.VirtualAddress;
            key[1] = section.Misc.VirtualSize ^ 0x67676767u;
            key[2] = section.SizeOfRawData;
            key[3] = ~section.Characteristics ^ 0x10101010u;

            ShimDecryptData(key, nonce, (uint8_t*)(&__ImageBase) + section.VirtualAddress, section.Misc.VirtualSize);
        }
    }

    return TRUE;
}
#pragma code_seg(pop)

#pragma code_seg(push, "MVM")
namespace Stages
{
bool Stage5(const std::string_view& aKey)
{
    __try
    {
        // This will trip up debugger too :(
        __debugbreak();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // I wanted CRC32 here but it's a pain in the ass :(
        uint32_t buf{};

        for (auto i = 0u; i < aKey.size(); i++)
        {
            if (aKey[i] & (1 << 2))
            {
                buf |= (1 << i);
            }
        }

        if constexpr (DebugKey)
        {
            std::printf("Stage5, buf: %x\n", buf);
        }

        return buf == 0x230e29;
    }

    return false;
}

bool Stage4(const std::string_view& aKey)
{
    __try
    {
        using FuncType = void (*)();

        ((FuncType)(aKey.data()))();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // Evaluates to 220222014256 (0CAF3\x00\x00\x00)
        // Generated with https://plzin.github.io/mba-wasm/
        const auto generatedMba = [](uint64_t aux0, uint64_t aux1, uint64_t aux2, uint64_t aux3,
                                     uint64_t aux4) -> uint64_t
        {
            return 8094817977203436890 * ~aux2 + 7510193739190165091 * aux2 +
                   14366305047014811836 * (aux0 ^ aux2 & aux0 & (aux0 | aux2)) +
                   9772327241882123071 * (aux2 & aux4 & aux2) + 13459951860894630346 * (aux4 ^ aux1 & aux1 & ~aux1) +
                   13686125441192878315 * aux3 + 7740132111024669138 * (aux0 ^ ~(aux1 & aux4)) +
                   8674416831827428545 * ~((aux2 | aux4) ^ aux3 ^ aux3) +
                   411862704142837778 * ((aux0 | aux4) & aux3 & aux3 ^ ~aux3) +
                   16097564094642208104 * (aux4 | aux0 & aux4 ^ aux1) + 5921076933617680677 * aux4 +
                   4462753404338102507 * ~(aux3 | ~aux0) +
                   18411075040723627080 * ((aux2 & aux1 | aux2 | aux0) ^ ~aux4 ^ aux4) +
                   13717958579293847517 * (~aux0 & (aux1 & aux4 ^ aux1)) +
                   14395853373514286887 * (aux3 & ~aux0 & ~~aux3) + 13983990669371449109 * ~(~aux4 ^ aux0 ^ aux4) +
                   15435397457100586577 * (aux1 ^ ~~aux0) + 9223372036854775808 * ~~(aux3 ^ aux2) +
                   14401974080000736372 * (~~aux2 | ~(aux0 & aux0)) +
                   18034881369566713838 * ~(aux0 & aux4 & aux0 & aux3) +
                   8357431983617538966 * ((aux3 | ~aux3) ^ (aux4 & aux1 | aux4 | aux1)) +
                   7695265346075917439 * ~((aux1 ^ aux0) & (aux0 | aux4)) +
                   411862704142837778 * ~((aux4 | aux4) & (aux3 | aux3));
        };

        constexpr auto SliceStart = 18u;

        auto keySlice = aKey.substr(SliceStart, 5);

        const auto constant = generatedMba(keySlice[0], keySlice[1], keySlice[2], keySlice[3], keySlice[4]);
        auto asStr = std::string_view((const char*)&constant);

        if constexpr (DebugKey)
        {
            std::printf("Stage4, asStr: %s\n", asStr.data());
        }

        if (keySlice.size() != asStr.size())
        {
            return false;
        }

        for (auto i = 0u; i < asStr.size(); i++)
        {
            if (std::tolower(keySlice[i]) != std::tolower(asStr[i]))
            {
                return false;
            }
        }

        return Stages::Stage5(aKey);
    }
    return false;
}

bool Stage3(const std::string_view& aKey)
{
    __try
    {
        // Boring access violation...
        volatile void* dummyAddress = (void*)(0x69690000ull);

        constexpr const char DummyString[] = "The flag is definitely at 0x69690000, don't overwrite it...";

        memcpy((void*)(dummyAddress), DummyString, sizeof(DummyString));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // Example candidate: '31337'
        constexpr auto SliceStart = 12u;

        auto keySlice = aKey.substr(SliceStart, 5);

        int sum{};

        for (auto ch : keySlice)
        {
            sum += ch;
        }

        if constexpr (DebugKey)
        {
            std::printf("Stage3, sum: %d\n", sum);
        }

        if (sum != 257)
        {
            return false;
        }

        return Stages::Stage4(aKey);
    }

    return false;
}

bool Stage2(const std::string_view& aKey)
{
    // Only 1 byte is used here because we want free space for CRC32 manipulation
    constexpr auto SliceStart = 6u;

    __try
    {
        volatile auto ch = aKey[SliceStart];
        volatile auto shouldBeZero = ch ^ '9';

        if constexpr (DebugKey)
        {
            std::printf("Stage2, ch: %c\n", ch);
        }

        if (!shouldBeZero)
        {
            __writecr3(0x69420000ull); // Dummy write to CR3 to throw privileged insn
                                       // exception
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return Stages::Stage3(aKey);
    }

    return false;
}

bool Stage1(const std::string_view& aKey)
{
    __try
    {
        // Avoid optimization, we will be dividing this by 0!
        volatile auto keySize = aKey.size();
        volatile auto targetKeySize = 23u;
        volatile auto ratio = 100u / (keySize - targetKeySize);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        constexpr auto SliceStart = 0u;
        // Check for basic key constraints
        for (auto i = 0u; i < aKey.size(); i++)
        {
            auto isDelim = (i == 5 || i == 11 || i == 17);
            if (isDelim && aKey[i] != '-')
            {
                return false;
            }
            else if (!isDelim)
            {
                if (!std::isalnum(aKey[i]))
                {
                    return false;
                }
            }
        }

        std::string_view funcSlice = aKey.substr(SliceStart, 5);

        if (std::tolower(funcSlice[0]) != 'd' || std::tolower(funcSlice[1]) != 'i' || std::tolower(funcSlice[2]) != 's')
        {
            if constexpr (DebugKey)
            {
                std::puts("Stage1, wtf?");
            }
            return false;
        }

        return Stages::Stage2(aKey);
    }
    return false;
}
} // namespace Stages

/**
 \brief Check the license key provided by the user.

 \param aKey The license key to check. Format: DIS01-23456-789AB-CDEF0, 4 blocks
 of 5 alnum chars separated by dashes. It's a bit silly
*/
extern "C" __declspec(dllexport) bool CheckLicenseKey(const char* aKey)
{
    __try
    {
        __ud2();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        std::string_view keyView = aKey;
        return Stages::Stage1(keyView);
    }
    return false;
}
#pragma code_seg(pop)
