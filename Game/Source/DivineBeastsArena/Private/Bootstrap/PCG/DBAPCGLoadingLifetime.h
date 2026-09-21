#pragma once

namespace DBA::PCG
{
/** 只描述Loading尝试的所有权交接；不代表PCG生成成功或原生清理完成。 */
class FLoadingLifetime
{
public:
    /** 仅实际RequestGeneration接纳后调用；Start失败不能取得其他请求所有权。 */
    constexpr void AcceptRequest() { if (!bIsReleased) { bHasRequest = true; } }
    /** 仅Poll核验真实Retained、成功及有效输出后调用。 */
    constexpr void ObserveSuccess() { if (bHasRequest && !bIsReleased) { bHasSucceeded = true; } }
    /** 返回是否应归还PCG请求；成功输出归世界组件保留，重复调用不重复清理。 */
    constexpr bool ReleaseTask()
    {
        if (bIsReleased) { return false; }
        bIsReleased = true;
        return bHasRequest && !bHasSucceeded;
    }
    constexpr bool IsReleased() const { return bIsReleased; }
    constexpr bool HasSucceeded() const { return bHasSucceeded; }
private:
    bool bHasRequest = false;
    bool bHasSucceeded = false;
    bool bIsReleased = false;
};
}
