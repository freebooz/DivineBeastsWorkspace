// 世界无关的主题请求资格值对象，供实际服务和自动化测试共用，没有全局计数。
#pragma once
#include "CoreMinimal.h"

struct FGamePlatformUIThemeRequestState
{
    /** 无有效GUID即无在途请求；关闭后拒绝恢复。 */
    FGuid RequestId;
    bool bClosed = false;
    FGuid Begin()
    {
        if (bClosed) return {};
        RequestId = FGuid::NewGuid();
        return RequestId;
    }
    bool CanComplete(const FGuid& Id) const
    { return !bClosed && Id.IsValid() && Id == RequestId; }
    bool Cancel(const FGuid& Id)
    {
        if (!CanComplete(Id)) return false;
        RequestId.Invalidate();
        return true;
    }
    void Close() { bClosed = true; RequestId.Invalidate(); }
};
