#pragma once
#include "CoreMinimal.h"

/** 本地鼠标手势栅栏；只保存捕获指针身份，失活／丢失捕获立即取消，不拥有角色或世界状态。 */
struct FDivineBeastsPreviewDragState
{
    bool bDragging = false;
    uint32 UserIndex = 0;
    uint32 PointerIndex = 0;
    void Begin(uint32 User, uint32 Pointer) { UserIndex=User; PointerIndex=Pointer; bDragging=true; }
    void Cancel() { bDragging=false; UserIndex=0; PointerIndex=0; }
    bool Owns(uint32 User, uint32 Pointer) const { return bDragging && UserIndex==User && PointerIndex==Pointer; }
    /** Slate逻辑像素转角度；异常数值不传播，单个输入事件最多90度，避免重新捕获时跳转。 */
    float YawForMove(uint32 User, uint32 Pointer, float DeltaX) const
    {
        return Owns(User,Pointer) && FMath::IsFinite(DeltaX) ? FMath::Clamp(DeltaX * 0.35f, -90.0f, 90.0f) : 0.0f;
    }
};
