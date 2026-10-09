#pragma once
#include <cstddef>
#include <cstdint>

// 导航服务器请求的纯值接纳和回调匹配策略。由世界服务在游戏线程使用；不保存对象或网络权威状态。
namespace GamePlatformNavigationRequestPolicy
{
inline bool CanSchedule(std::size_t ActiveCount, std::size_t Limit, bool DuplicateInFlight)
{ return !DuplicateInFlight && ActiveCount < Limit; }
inline bool MatchesTimeout(std::uint64_t ActualOperation, std::uint64_t ExpectedOperation,
    std::int32_t ActualWorld, std::int32_t ExpectedWorld)
{ return ActualOperation == ExpectedOperation && ActualWorld == ExpectedWorld; }
}
