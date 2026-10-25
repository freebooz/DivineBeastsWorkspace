#include "API/GamePlatformWeatherWindSource.h"
#include "Engine/WindDirectionalSource.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "EngineUtils.h"

const FName& GamePlatformWeatherWindSource::Tag()
{
    static const FName Value(TEXT("GamePlatform.Weather.GlobalWind"));
    return Value;
}
EGamePlatformSceneWindResult GamePlatformWeatherWindSource::Read(
    UWorld& World, FGamePlatformWeatherState& State, AWindDirectionalSource*& Source, FString& Error)
{
    check(IsInGameThread()); Source = nullptr; Error.Reset();
    for (TActorIterator<AWindDirectionalSource> It(&World); It; ++It)
    {
        if (!IsValid(*It) || !It->ActorHasTag(Tag())) continue;
        if (Source) { Source = nullptr; Error = TEXT("DuplicateGlobalWindSource"); return EGamePlatformSceneWindResult::Invalid; }
        Source = *It;
    }
    if (!Source) return EGamePlatformSceneWindResult::Absent;
    const auto* Component = Source->GetComponent();
    auto Candidate = State;
    const auto Rotation = Source->GetActorRotation();
    if (!IsValid(Component) || Component->bPointWind || !FMath::IsFinite(Rotation.Pitch) ||
        !FMath::IsNearlyZero(FRotator::NormalizeAxis(Rotation.Pitch), .001f))
    { Source = nullptr; Error = TEXT("GlobalWindMustBeHorizontalDirectional"); return EGamePlatformSceneWindResult::Invalid; }
    Candidate.WindYawDegrees = FRotator::ClampAxis(Rotation.Yaw);
    if (Candidate.WindYawDegrees >= 360.f) Candidate.WindYawDegrees = 0.f; // float舍入后的整圈仍为0°。
    Candidate.WindIntensity = Component->Strength;
    Candidate.WindGustStrength = Component->MaxGustAmount;
    Candidate.WindGustFrequencyHz = Component->Speed;
    if (!Candidate.IsValid())
    { Source = nullptr; Error = TEXT("InvalidGlobalWindParameters"); return EGamePlatformSceneWindResult::Invalid; }
    State = Candidate;
    return EGamePlatformSceneWindResult::Applied;
}
