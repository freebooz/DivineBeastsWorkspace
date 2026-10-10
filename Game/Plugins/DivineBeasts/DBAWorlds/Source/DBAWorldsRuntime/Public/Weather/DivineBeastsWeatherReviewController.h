// 神兽联盟第三层天气审核Actor：只用于真实地图/PIE人工审核，复用平台唯一天气权威子系统。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "DivineBeastsWeatherReviewController.generated.h"

/**
 * ADivineBeastsWeatherReviewController（神兽联盟天气审核控制Actor）。
 *
 * 这是可派生蓝图的测试场景组件，不接管世界天气、网络Actor或粒子播放。
 * 只有权威World中经关卡配置bApplyAtBeginPlay显式同意才提交天气；
 * 不能被客户端调用来更改服务器结果，禁止用于正式竞技地图。
 * 建议蓝图：/Game/Development/Weather/BP_DBA_WeatherReviewController。
 */
UCLASS(Blueprintable)
class DBAWORLDSRUNTIME_API ADivineBeastsWeatherReviewController : public AActor
{
    GENERATED_BODY()
public:
    ADivineBeastsWeatherReviewController();

    /** 只读读取服务器或客户端当前天气；失败时还原默认晴天。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Weather|Review")
    bool ReadCurrentWeather(FGamePlatformWeatherState& OutWeather) const;

    /** 人工审核中的天气切换，必须是权威世界；不创建RPC。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Weather|Review")
    bool ApplyReviewWeather(EGamePlatformWeatherType Type, float TransitionDurationSeconds);

protected:
    virtual void BeginPlay() override;

    /** 只有明确配置的开发审核地图才在开始时触发一次天气；默认关闭。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Weather|Review")
    bool bApplyAtBeginPlay = false;

    /** 测试默认天气。正式天气规则仍由DBAWorlds GameMode/数据定义负责。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Weather|Review")
    EGamePlatformWeatherType PreviewWeather = EGamePlatformWeatherType::LightRain;

    /** 过渡秒数，单次申请由平台天气子系统再次校验。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|Weather|Review",
        meta=(ClampMin="0", ClampMax="60", Units="s"))
    float PreviewTransitionSeconds = 8.f;
};
