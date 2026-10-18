// DivineBeasts项目客户端创建/选择页共享鼠标手势状态；由页面按输入事件读取，不接触网络或角色权威。
#pragma once
#include "CoreMinimal.h"

/** 本地鼠标手势栅栏；只保存捕获指针身份，失活／丢失捕获立即取消，不拥有角色或世界状态。 */
struct FDivineBeastsPreviewDragState
{
    /** 是否处于已捕获的拖动；默认false，页面失活或丢失捕获时由调用方取消。 */
    bool bDragging = false;
    /** 捕获时的本地Slate用户索引；0也是合法用户，是否有效由bDragging决定。 */
    uint32 UserIndex = 0;
    /** 捕获时的指针索引；0也是合法指针，取消后与用户索引一同清零。 */
    uint32 PointerIndex = 0;
    /** 输入捕获成功后记录用户与指针身份，不自行取得Slate捕获或创建角色。 */
    void Begin(uint32 User, uint32 Pointer) { UserIndex=User; PointerIndex=Pointer; bDragging=true; }
    /** 页面失活、释放或丢失捕获时清除手势身份；重复取消无副作用。 */
    void Cancel() { bDragging=false; UserIndex=0; PointerIndex=0; }
    /** 仅已捕获的同一用户和指针可驱动预览；其他输入与取消后的旧事件返回false。 */
    bool Owns(uint32 User, uint32 Pointer) const { return bDragging && UserIndex==User && PointerIndex==Pointer; }
    /**
     * DeltaX为Slate横向逻辑像素位移，返回本次预览Yaw增量（度），每像素0.35度。
     * 向右拖动（正DeltaX）产生负Yaw，向左拖动产生正Yaw；零位移、非有限值或非捕获指针返回0。
     * 单个输入事件限幅到[-90,90]度，避免重新捕获时坐标突变；不修改捕获状态或权威角色。
     */
    float YawForMove(uint32 User, uint32 Pointer, float DeltaX) const
    {
        return Owns(User,Pointer) && FMath::IsFinite(DeltaX) ? FMath::Clamp(DeltaX * -0.35f, -90.0f, 90.0f) : 0.0f;
    }
};


/** 角色展示区滚轮映射；正向滚轮缩短视距，按住Shift时增加焦距。 */
struct FDivineBeastsPreviewCameraWheelState
{
    /** 普通滚轮每格移动30厘米，单次事件限幅120厘米。 */
    static float DistanceDelta(float WheelDelta)
    {
        return FMath::IsFinite(WheelDelta)
            ? FMath::Clamp(-WheelDelta * 30.0f, -120.0f, 120.0f)
            : 0.0f;
    }

    /** Shift加滚轮每格调整5毫米，单次事件限幅20毫米。 */
    static float FocalLengthDelta(float WheelDelta)
    {
        return FMath::IsFinite(WheelDelta)
            ? FMath::Clamp(WheelDelta * 5.0f, -20.0f, 20.0f)
            : 0.0f;
    }
};
