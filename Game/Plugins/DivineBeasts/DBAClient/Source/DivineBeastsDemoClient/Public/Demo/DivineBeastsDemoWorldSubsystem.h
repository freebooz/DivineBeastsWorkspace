// 开发演示世界交互控制：只使用真实BirdFlock和Weather审核Actor公开API，不模拟播放或修改服务器权威。
// 所有临时参数按World作用域保存；世界关闭前恢复/释放，禁止跨世界使用旧Actor。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "Types/GamePlatformDataLease.h"
#include "Environment/DivineBeastsBirdFlockFlightSettings.h"
#include "DivineBeastsDemoWorldSubsystem.generated.h"

class UDivineBeastsBirdFlockSceneComponent;
class UDivineBeastsBirdFlockBatchComponent;
class ADivineBeastsWeatherReviewController;
class UDivineBeastsDemoHubWidget;

/**
 * 神兽联盟插件演示世界运行上下文。DevClient/Editor且显式DBAPluginDemo时才创建。
 * 游戏世界内主动管理真效果、演示UI和资源租约；不在Server、正式流程或Commandlet创建。
 */
UCLASS()
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    /** 仅修改正式BirdFlockSceneComponent并触发Presentation/Data/VFX链路，不再调用旧ISM。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdCount(int32 Count);

    /** 调整正式鸟群场景配置的质量比例，实际GPU可见数仍由Niagara EffectType决定。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdQuality(float Fraction);
    /** 切换正式鸟群物种配置，未知物种由项目表现目录拒绝，失败回退当前配置。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdSpecies(FName SpeciesId);

    /** 可选路径模式；仅Spline模式需同世界有效Spline组件与完成相应Niagara资源。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdFlightMode(EDivineBeastsBirdFlockFlightMode Mode);

    /** 标准Boids三权重，若实际资源Schema/邻域模块未就绪则明确失败。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdFlocking(float SeparationWeight, float AlignmentWeight, float CohesionWeight);

    /** 动态切换滑翔混合；0保留原材质WPO，不代表VAT资产存在。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdGlide(float GlideBlend);

    /** 批次控制只持本组件Owner/Token，不会清除其它插件演示群。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool StartBirdBatches();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    void StopBirdBatches();

    /** 真实天气审核Actor仅在世界权威上下文允许；不能以本函数绕过专服。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|Weather")
    bool ApplyWeather(EGamePlatformWeatherType Weather, float TransitionSeconds);

    /** 恢复本世界自演示进入后修改过的鸟群数值与天气，不影响其他实例。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo")
    bool ResetDemoChanges();

    /** -1表示当前未做真实Niagara GPU读回；绝不返回旧ISM计数冒充真实鸟数。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo|VFX")
    int32 GetActualBirdCount() const;
    /** 已受理的项目正式服务鸟数（请求数），不代表GPU现存粒子数。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo|VFX")
    int32 GetRequestedBirdCount() const;

    /** 当前世界正式鸟群场景组件数量，不是逐鸟Actor数量。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo|VFX")
    int32 GetBirdActorCount() const;

    /** 当前世界演示请求结果，供UI显示，不存储敏感信息。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    FText GetLastFeedback() const { return LastFeedback; }

private:
    /** 演示修改前的正式组件快照；失败时原位回滚，离开世界不访问已失效组件。 */
    struct FOriginalBirdState
    {
        int32 Count = 0;
        FName Species = NAME_None;
        bool bAdvanced = false;
        FDivineBeastsBirdFlockFlightSettings Flight;
    };
    TMap<TWeakObjectPtr<UDivineBeastsBirdFlockSceneComponent>, FOriginalBirdState> OriginalBirds;
    /** 首次可写操作前记录原状态，离开世界只恢复本演示改变的组件。 */
    void CaptureOriginalBird(UDivineBeastsBirdFlockSceneComponent& Bird);
    /** 执行正式Apply并在失败时还原用户可编辑字段。 */
    bool CommitBirdChange(UDivineBeastsBirdFlockSceneComponent& Bird,
        const FOriginalBirdState& Before, FString& OutError);
    /** 有界同步演示操作：保存原值、实际调用正式VFX链路、失败恢复本地可编辑字段。 */
    bool ApplyBirdEdits(const TCHAR* Description,
        TFunctionRef<void(UDivineBeastsBirdFlockSceneComponent&)> Edit);
    TWeakObjectPtr<ADivineBeastsWeatherReviewController> OriginalWeatherActor;
    EGamePlatformWeatherType OriginalWeatherType = EGamePlatformWeatherType::Clear;
    bool bHasOriginalWeather = false;

    /** GamePlatformData（平台数据）颁发的普通资源租约，退出只释放本世界所持身份。 */
    FGamePlatformDataLease UILease;
    TWeakObjectPtr<UDivineBeastsDemoHubWidget> ActiveWidget;
    TWeakObjectPtr<UWorld> SourceWorld;
    FText LastFeedback;

    void RequestDemoOverlay(UWorld& World);
    void OnOverlayAssetReady(FSoftObjectPath WidgetClassPath, const FGamePlatformDataLease& Lease,
        bool bSucceeded);
    void ReleaseDemoOverlay();
};
