// 平台中立字节摘要；不拥有文件、业务规则或证据资格。
#pragma once
#include "CoreMinimal.h"
namespace GamePlatform::Hash
{
    /** 同步散列调用者拥有的ByteSize字节，输出64位小写SHA256十六进制。
     * 任意线程可用；Data在调用期间须保持有效且不变。空输入允许nullptr；非空nullptr或长度
     * 无法由本机size_t表示时失败并清空输出。失败不产生替代摘要；摘要不证明来源或人工验收。
     * 不进行文件I/O，不保留输入引用；输出不得与输入缓冲区别名。
     */
    GAMEPLATFORMCORE_API bool TrySha256Hex(const void* Data, uint64 ByteSize, FString& OutHex);
}
