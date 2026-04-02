#include <phnt_windows.h>
#include <phnt.h>

#include <stdbool.h>
#include <winnt.h>
#include <winuser.h>

volatile BOOL ShouldDumpFlag = FALSE;

// Note: overwritten with real flag by protector filter driver in memory...
volatile CHAR FlagString[] =
    "fake_flag_fake_flag_fake_flag_fake_flag_fake_flag_fake_flag_fake_flag_fake_flag_fake_flag_fake_flag";

const char Msg[] = "I'm protected by finest of FlagAntiCheat technology! Don't even try to open a handle to me...\n";

int main()
{
    PPEB peb = NtCurrentPeb();
    HANDLE stdoutHandle = peb->ProcessParameters->StandardOutput;

    while(!ShouldDumpFlag) {
        WriteFile(stdoutHandle, Msg, sizeof(Msg), NULL, NULL);
        Sleep(1000u);
    }

    MessageBoxA(0u, (CHAR*)(FlagString), "You are a real winner!", MB_ICONQUESTION | MB_TOPMOST | MB_SYSTEMMODAL);

    return 0;
}
