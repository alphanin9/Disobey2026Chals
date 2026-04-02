#pragma once
#include <cstdint>

// This is here because XMake is dumb sometimes and doesn't let us set language for packages
// So tiny-aes-c compiles with C++ compiler...

extern "C" {
    void ShimDecryptData(void* aKey, void* aNonce, void* aData, size_t aSize);
}
