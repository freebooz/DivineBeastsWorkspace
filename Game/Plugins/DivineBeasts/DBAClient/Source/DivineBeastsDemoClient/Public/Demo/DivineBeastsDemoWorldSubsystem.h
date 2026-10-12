// 开发演示世界交互控制：只使用真实BirdFlock和Weather审核Actor公开API，不模拟播放或修改服务器权威。
// 所有临时参数按World作用域保存；世界关闭前恢复/释放，禁止跨世界使用旧Actor。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "Types/GamePlatformDataLease.h"
#include "DivineBeastsDemoWorldSubsystem.generated.h"

class AGamePlatformBirdFlockActor;
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

    /** 真实鸟群Actor的数量调整；找到并成功更新至少一个对象才返回true。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdCount(int32 Count);

    /** 对有真实鸟群的世界调整画质比例；范围0..1，拒绝超出或无Actor的调用。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdQuality(float Fraction);

    /** 真实天气审核Actor仅在世界权威上下文允许；不能以本函数绕过专服。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|Weather")
    bool ApplyWeather(EGamePlatformWeatherType Weather, float TransitionSeconds);

    /** 恢复本世界自演示进入后修改过的鸟群数值与天气，不影响其他实例。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo")
    bool ResetDemoChanges();

    /** 已实际被鸟群Actor绘制的数量；无Actor时为0，不等同已成功生成。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo|VFX")
    int32 GetActualBirdCount() const;

    /** 当前世界真实Actor数量，不含静态贴图和其它实例。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo|VFX")
    int32 GetBirdActorCount() const;

    /** 当前世界演示请求结果，供UI显示，不存储敏感信息。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    FText GetLastFeedback() const { return LastFeedback; }

private:
    /** 只存储本世界实际操作过的Actor原值；其它插件若在外部改变它不由本控制器撤销。 */
    TMap<TWeakObjectPtr<AGamePlatformBirdFlockActor>, int32> OriginalCounts;
    TMap<TWeakObjectPtr<AGamePlatformBirdFlockActor>, float> OriginalQuality;
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
