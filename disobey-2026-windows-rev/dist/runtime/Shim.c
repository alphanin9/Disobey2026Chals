#include <aes.h>
#include <stdint.h>

void ShimDecryptData(void* aKey, void* aNonce, void* aData, size_t aSize) {
    struct AES_ctx ctx;

    AES_init_ctx_iv(&ctx, (uint8_t*)(aKey), (uint8_t*)(aNonce));

    AES_CBC_decrypt_buffer(&ctx, (uint8_t*)aData, aSize);
}
