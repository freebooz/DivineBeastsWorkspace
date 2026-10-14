// 水体视觉子系统只在客户端/PIE的游戏世界创建。
// 提交的涟漪不参与服务器水深判定，不复制粒子、RenderTarget或每帧Surface状态。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformWaterTypes.h"
#include "GamePlatformWaterClientWorldSubsystem.generated.h"

class UNiagaraSystem;
class UGamePlatformWaterPresentationPreset;
struct FStreamableHandle;

/** UGamePlatformWaterClientWorldSubsystem（客户端水体表现），每个UWorld单独管理资源租约。 */
UCLASS()
class GAMEPLATFORMWATERCLIENT_API UGamePlatformWaterClientWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Deinitialize() override;

    /** 只接收项目通过GamePlatformData有效租约装配的预设；异步加载并在世界卸载时取消。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Water")
    bool ConfigurePresentation(UGamePlatformWaterPresentationPreset* Preset);

    /** 请求本地水波：先查平台权威几何，然后做预算/距离过滤，无可见资源时返回false。
     * bSplash为true请求附加飞溅；即使没有视觉效果也不反馈修改Gameplay。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Water")
    bool RequestLocalRipple(const FVector& WorldPosition, float Strength, bool bSplash);

    /** 只在观察/审核时调用：官方WaterBody的数量不代表材质已经正确显示。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Water")
    int32 GetNativeWaterBodyCount() const;

    /** 清理已发资源句柄和帧内请求预算；退出世界和更换预设时幂等。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Water")
    void ClearPresentation();

private:
    TWeakObjectPtr<UGamePlatformWaterPresentationPreset> PresetLease;
    TSharedPtr<FStreamableHandle> PendingLoad;
    TArray<double> RecentRippleTimes;
    FVector LastRippleLocation = FVector::ZeroVector;
    double LastRippleTime = -100.0;
    uint64 Generation = 0;
};
