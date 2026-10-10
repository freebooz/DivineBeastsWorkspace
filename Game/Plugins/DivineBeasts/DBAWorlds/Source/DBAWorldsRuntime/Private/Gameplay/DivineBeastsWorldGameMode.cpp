// 项目共享GameMode只指定项目控制器；认证和生成仍由平台唯一门禁拥有。
#include "Gameplay/DivineBeastsWorldGameMode.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Engine/World.h"
ADivineBeastsWorldGameMode::ADivineBeastsWorldGameMode()
{
    PlayerControllerClass=ADivineBeastsWorldPlayerController::StaticClass();
}

void ADivineBeastsWorldGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority() || !GetWorld()) return;
    UGamePlatformWeatherWorldSubsystem* Weather = GetWorld()->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();
    if (!Weather || !Weather->ActivateWeather(InitialWeather))
    {
        UE_LOG(LogTemp, Warning, TEXT("神兽联盟世界天气启用失败：检查服务器模式和天气初始状态。"));
        return;
    }
    if (bEnableWeatherSchedule &&
        (!Weather->ConfigureSchedule(WeatherSchedule, static_cast<int32>(GetWorld()->GetUniqueID())) ||
         !Weather->StartSchedule()))
    {
        UE_LOG(LogTemp, Warning, TEXT("神兽联盟自动天气表无效，保持初始化天气；请检查时长、权重、强度和内容版本。"));
    }
}

