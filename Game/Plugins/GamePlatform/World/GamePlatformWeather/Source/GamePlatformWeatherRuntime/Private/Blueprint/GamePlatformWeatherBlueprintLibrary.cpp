// 天气蓝图API只解析本世界Subsytem，不保存跨世界状态；实际写入仍走平台服务器权威入口。
#include "Blueprint/GamePlatformWeatherBlueprintLibrary.h"

#include "Definitions/GamePlatformWeatherPresetDefinition.h"
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

namespace
{
    UGamePlatformWeatherWorldSubsystem* ResolveService(const UObject* Context)
    {
        if (!IsValid(Context)) return nullptr;
        UWorld* World = Context->GetWorld();
        if (!IsValid(World) || World->bIsTearingDown) return nullptr;
        return World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();
    }
}

bool UGamePlatformWeatherBlueprintLibrary::ReadWeather(
    const UObject* WorldContextObject, FGamePlatformWeatherState& OutWeather)
{
    OutWeather = FGamePlatformWeatherState();
    if (!IsInGameThread()) return false;
    UGamePlatformWeatherWorldSubsystem* Service = ResolveService(WorldContextObject);
    if (!Service) return false;

    const FGamePlatformWeatherSnapshot Current = Service->GetCurrentSnapshot();
    if (Current.Revision <= 0) return false;
    const UWorld* World = Service->GetWorld();
    if (!World) return false;
    const AGameStateBase* State = World->GetGameState();
    const float ServerSeconds = State
        ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
    OutWeather = Current.Sample(ServerSeconds);
    return OutWeather.IsValid();
}

bool UGamePlatformWeatherBlueprintLibrary::SetWeatherOnAuthority(
    const UObject* WorldContextObject,
    const FGamePlatformWeatherState& TargetWeather, const float TransitionSeconds)
{
    if (!IsInGameThread() || !TargetWeather.IsValid()) return false;
    if (UGamePlatformWeatherWorldSubsystem* Service = ResolveService(WorldContextObject))
    {
        return Service->SetWeather(TargetWeather, TransitionSeconds);
    }
    return false;
}

bool UGamePlatformWeatherBlueprintLibrary::ApplyWeatherPresetOnAuthority(
    const UObject* WorldContextObject,
    const UGamePlatformWeatherPresetDefinition* Preset)
{
    if (!IsInGameThread() || !IsValid(Preset)) return false;
    if (UGamePlatformWeatherWorldSubsystem* Service = ResolveService(WorldContextObject))
    {
        return Service->ApplyPreset(Preset);
    }
    return false;
}
