#pragma once
#include "Framework/GamePlatformGameModeBase.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "DivineBeastsWorldGameMode.generated.h"

/** 第三层共享世界装配：复用平台准入/出生门禁，仅选择项目控制器。
 * 服务器可信Bootstrap完成World/Data装配后启动体验；客户端不依赖服务器私有实现。 */
UCLASS()
class DBAWORLDSRUNTIME_API ADivineBeastsWorldGameMode : public AGamePlatformGameModeBase
{
    GENERATED_BODY()
public:
    ADivineBeastsWorldGameMode();
protected:
    /** 正式世界GameMode启动后注册唯一平台天气Actor；客户端从复制快照驱动表现。 */
    virtual void BeginPlay() override;
    /** 世界初始天气仅由服务器读取；默认晴天，不在登录前端启动随机雨雪。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|Weather")
    FGamePlatformWeatherState InitialWeather;
    /** 是否按定义序列自动调度；默认关闭，避免竞技场随机关联天气影响公平性。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|Weather")
    bool bEnableWeatherSchedule = false;
    /** 项目世界自己的天气策略数据，运行时传给平台世界服务。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|Weather", meta=(EditCondition="bEnableWeatherSchedule"))
    TArray<FGamePlatformWeatherScheduleEntry> WeatherSchedule;
};
