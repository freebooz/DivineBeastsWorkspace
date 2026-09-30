// 客户端音频槽位接纳策略；只计数Pending与Active，租约所有权仍归WorldSubsystem。
#pragma once
struct FGamePlatformSFXBudgetPolicy
{
    static bool CanReserve(int Pending, int Active) { return Pending >= 0 && Active >= 0 && Pending < 128 && Active < 256 - Pending; }
};
