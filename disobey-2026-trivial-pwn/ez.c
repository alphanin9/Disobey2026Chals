#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

// gcc ez.c -std=c2x -O2 -no-pie -fno-stack-protector -D_FORTIFY_SOURCE=0 -o ez
// Cool things about this: blind pwn lol
// This is very sloppable tho

uint64_t exitcode = 1337; // /bin/sh goes here...

void timeout(int signal) {
    // Note: this doesn't work cuz exit() is not resolved at this point
    // Whatever, you can find your way out anyway
    // cuz you still have signal(), alarm(), setbuf(), read() and pause()
    exit(exitcode);
}

__attribute__((constructor))
void pre_main() {
    signal(SIGALRM, timeout);
    alarm(7);
}

__attribute((naked))
void gift() {
    __asm__(
        "addq %rsi, (%rdi)\n"
        "ret\n"
    );
}

__attribute((naked))
void gift2() {
    __asm__(
        "pop %rsi\n"
        "pop %rdi\n"
        "ret\n"
    );
}

// Note: not needed cuz our problem was that we screwed up with calling system()
__attribute((naked))
void gift3() {
    __asm__(
        "movq -0x70(%rsi), %rax\n"
        "xorq %rsi, %rsi\n"
        "xorq %rdx, %rdx\n"
        "ret\n"
    );
}

__attribute__((noinline))
void vuln() {
    char stack_buffer[28];
    fgets(stack_buffer, 280, stdin);
}

int main() {
    vuln();
    pause();
}
