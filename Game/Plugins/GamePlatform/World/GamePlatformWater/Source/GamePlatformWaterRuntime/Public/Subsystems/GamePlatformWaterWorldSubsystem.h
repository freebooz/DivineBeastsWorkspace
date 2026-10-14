// UGamePlatformWaterWorldSubsystem（平台权威水域世界服务）：区域增删由Actor生命周期触发，
// 不遍历所有Actor、不做GPU读回、不创建全局静态缓存；退出世界立即清理。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformWaterTypes.h"
#include "GamePlatformWaterWorldSubsystem.generated.h"

class AGamePlatformWaterRegion;

/** 水域查询只依赖本世界内的静态区域，不承担网络实例分配。 */
UCLASS()
class GAMEPLATFORMWATERRUNTIME_API UGamePlatformWaterWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    /** Editor工具地图不会自动启动服务器玩法子系统；PIE和正式游戏允许。 */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Deinitialize() override;

    /** 注册有效静态水域，拒绝跨World、重复RegionId及超额区域（最多128）；不修改地图对象。 */
    bool RegisterRegion(AGamePlatformWaterRegion* Region);
    /** 只解除当前Actor的注册，不撤销其他水域；允许重复调用。 */
    void UnregisterRegion(const AGamePlatformWaterRegion* Region);

    /** 无水时OutSample清空；重叠区域以高优先级优先，仍相同则选离水面最近的结果。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Water")
    bool QueryWaterAtLocation(const FVector& WorldLocation, FGamePlatformWaterSample& OutSample) const;

    UFUNCTION(BlueprintPure, Category="GamePlatform|Water")
    int32 GetRegisteredRegionCount() const { return Regions.Num(); }

private:
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AGamePlatformWaterRegion>> Regions;
};
