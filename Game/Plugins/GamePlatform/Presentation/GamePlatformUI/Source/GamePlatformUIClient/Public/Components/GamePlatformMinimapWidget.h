#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "Engine/Texture2D.h"
#include "GamePlatformMinimapWidget.generated.h"

/** FGamePlatformUIMinimapMarker（小地图标记）：位置为0～1 UV，不携带Actor指针。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIMinimapMarker
{
    GENERATED_BODY()

    /** 唯一标记身份，可用于蓝图增量更新。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    FName MarkerId = NAME_None;
    /** 归一化地图坐标；来源适配器负责世界坐标投影。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    FVector2D NormalizedPosition = FVector2D::ZeroVector;
    /** 通用类别：队友、目标、资源、交互等，不定义任何项目地图规则。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    FName CategoryId = NAME_None;
    /** 当前是否可见，由上层视野/权限事实决定。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    bool bVisible = true;
};

/** FGamePlatformUIMinimapState（小地图只读UI快照）。
 * 底图采用软纹理引用，项目层负责选择美术，平台不得同步加载。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIMinimapState
{
    GENERATED_BODY()

    /** 地图显示身份，不参与世界实例分配。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    FName MapId = NAME_None;
    /** 显示版本，同一个MapId严格递增。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    int64 Revision = -1;
    /** 仅用于视觉显示的软引用，不属于地形服务器烘焙资源。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    TSoftObjectPtr<UTexture2D> MapTexture;
    /** 玩家在地图中的归一化位置；不得据此判定权威位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    FVector2D PlayerNormalizedPosition = FVector2D::ZeroVector;
    /** 面向正北的偏转角度（度），由地图定位适配器转换。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    float HeadingDegrees = 0.0f;
    /** 可视缩放系数，限制在0.25～8。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    float Zoom = 1.0f;
    /** 最多128个可见对象标记；大型世界需要先裁剪后推送。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Minimap")
    TArray<FGamePlatformUIMinimapMarker> Markers;
};

/** UGamePlatformMinimapWidget（游戏通用小地图视觉组件）。
 * 只消费快照，不直接查询地图Actor、地理坐标、寻路或服务器视野。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformMinimapWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    /** 经过有限值/容量/版本/重复身份校验后替换快照；失败不触发视觉刷新。 */
    UFUNCTION(BlueprintCallable, Category="UI|Minimap")
    bool ApplyMinimapState(const FGamePlatformUIMinimapState& InState);

    /** 蓝图值读取接口，C++高频访问请用GetMinimapStateView避免数组复制。 */
    UFUNCTION(BlueprintPure, Category="UI|Minimap")
    FGamePlatformUIMinimapState GetMinimapState() const { return State; }

    const FGamePlatformUIMinimapState& GetMinimapStateView() const { return State; }

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Minimap",
        meta=(DisplayName="小地图投影已更新"))
    void BP_OnMinimapChanged(FGamePlatformUIMinimapState UpdatedState);

private:
    UPROPERTY(Transient)
    FGamePlatformUIMinimapState State;
};
