#pragma once
#include <cstdint>
// 职责：平台服务器HTTP准入接收预算判定；供真实接收回调与原生边界测试共同使用。
// 只判定容量，不解析凭据、不做网络授权；使用减法避免恶意超大长度加法溢出。
namespace GamePlatform::Server
{
/** 当前/新增/上限单位均为字节；零/负长度或已超限拒绝，恰好到上限允许。 */
inline bool CanAcceptAdmissionResponseBytes(std::int64_t currentBytes, std::int64_t incomingBytes, std::int64_t maximumBytes)
{
    return currentBytes >= 0 && incomingBytes > 0 && maximumBytes > 0 && currentBytes <= maximumBytes && incomingBytes <= maximumBytes - currentBytes;
}
}
