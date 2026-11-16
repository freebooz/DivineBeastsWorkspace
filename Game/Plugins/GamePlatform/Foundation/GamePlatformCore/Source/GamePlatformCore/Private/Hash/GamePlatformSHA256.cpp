#include "Hash/GamePlatformSHA256.h"
#include "String/BytesToHex.h"
THIRD_PARTY_INCLUDES_START
#include <openssl/sha.h>
THIRD_PARTY_INCLUDES_END

bool GamePlatform::Hash::TrySha256Hex(const void* Data, uint64 ByteSize, FString& OutHex)
{
    OutHex.Reset();
    if ((!Data && ByteSize != 0) || ByteSize > static_cast<uint64>(static_cast<SIZE_T>(-1))) return false;
    // 自有输出缓冲，禁止使用OpenSSL共享静态摘要；空输入也提供有效地址。
    const uint8 Empty = 0;
    uint8 Digest[SHA256_DIGEST_LENGTH];
    if (!SHA256(Data ? static_cast<const uint8*>(Data) : &Empty, static_cast<size_t>(ByteSize), Digest)) return false;
    TCHAR Hex[SHA256_DIGEST_LENGTH * 2];
    UE::String::BytesToHexLower(MakeArrayView(Digest), Hex);
    OutHex = FString(UE_ARRAY_COUNT(Hex), Hex);
    return true;
}
