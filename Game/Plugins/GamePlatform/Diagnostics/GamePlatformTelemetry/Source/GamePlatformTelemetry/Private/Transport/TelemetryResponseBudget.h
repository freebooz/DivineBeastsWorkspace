#pragma once
#include <cstdint>
// 平台遥测内部HTTP接收策略：只计数并丢弃无需解析的确认正文，由传输器拥有本请求预算。
// 使用减法比较避免恶意超大分块令计数溢出；超限必须失败，不能把截断响应当成功。
namespace GamePlatform::Telemetry
{
inline bool CanReceiveBytes(std::int64_t ReceivedBytes, std::int64_t ChunkBytes, std::int64_t LimitBytes)
{
    return ReceivedBytes >= 0 && ChunkBytes >= 0 && LimitBytes >= 0 &&
        ReceivedBytes <= LimitBytes && ChunkBytes <= LimitBytes - ReceivedBytes;
}
}
