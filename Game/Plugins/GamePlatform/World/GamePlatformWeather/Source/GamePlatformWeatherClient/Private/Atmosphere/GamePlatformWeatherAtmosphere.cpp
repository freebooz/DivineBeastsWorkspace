#include "Atmosphere/GamePlatformWeatherAtmosphere.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/ExponentialHeightFog.h"
#include "EngineUtils.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformWeatherAtmosphere, Log, All);

FGamePlatformWeatherAtmosphere::FGamePlatformWeatherAtmosphere(UWorld& InWorld,
    const FGamePlatformWeatherAtmosphereProfile& InProfile) : World(&InWorld), Profile(InProfile)
{
    check(IsInGameThread());
    if (!Profile.bEnabled || InWorld.GetNetMode() == NM_DedicatedServer) return;
    for (TActorIterator<AActor> It(&InWorld); It; ++It) CaptureActor(*It);
    EnsureLocalFog();
    LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddRaw(this,
        &FGamePlatformWeatherAtmosphere::OnLevelAdded);
    LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddRaw(this,
        &FGamePlatformWeatherAtmosphere::OnLevelRemoved);
}

void FGamePlatformWeatherAtmosphere::EnsureLocalFog()
{
    Fogs.RemoveAll([](const auto& Entry) { return !Entry.Component.IsValid(); });
    if (Fogs.IsEmpty() && World.IsValid())
    {
        FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Params.OverrideLevel = World->PersistentLevel;
        if (auto* Fog = World->SpawnActor<AExponentialHeightFog>(Params))
        {
            Fog->SetReplicates(false);
            Fog->GetComponent()->SetFogDensity(0.f);
            Fog->GetComponent()->SetStartDistance(FMath::Max(Profile.FogStartDistanceCm, 0.f));
            OwnedFog = Fog;
            CaptureActor(Fog);
        }
    }
}

void FGamePlatformWeatherAtmosphere::CaptureActor(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    TInlineComponentArray<ULightComponentBase*> Components(Actor);
    for (auto* Light : Components)
    {
        const bool bSky = Light->IsA<USkyLightComponent>();
        if ((!bSky && !Light->IsA<UDirectionalLightComponent>()) ||
            Light->Mobility == EComponentMobility::Static ||
            Lights.ContainsByPredicate([Light](const auto& Entry) { return Entry.Component == Light; })) continue;
        Lights.Add({Light, Light->Intensity, FLinearColor(Light->LightColor), bSky});
    }
    TInlineComponentArray<UExponentialHeightFogComponent*> FogComponents(Actor);
    for (auto* Fog : FogComponents)
    {
        if (Fogs.ContainsByPredicate([Fog](const auto& Entry) { return Entry.Component == Fog; })) continue;
        Fogs.Add({Fog, Fog->FogDensity, Fog->StartDistance, Fog->FogInscatteringLuminance});
    }
}

void FGamePlatformWeatherAtmosphere::OnLevelAdded(ULevel* Level, UWorld* AddedWorld)
{
    if (!Level || AddedWorld != World.Get() || AddedWorld->bIsTearingDown) return;
    for (AActor* Actor : Level->Actors) CaptureActor(Actor);
    // 流送来的真实雾接管后移除本适配器的临时雾；不允许两个重叠高度雾竞争。
    if (Fogs.Num() > 1 && OwnedFog.IsValid())
    {
        auto* Component = OwnedFog->GetComponent();
        Fogs.RemoveAll([Component](const auto& Entry) { return Entry.Component == Component; });
        OwnedFog->Destroy(); OwnedFog.Reset();
    }
    const float Cloud = FMath::Max(LastCloud, 0.f), Fog = FMath::Max(LastFog, 0.f);
    LastCloud = LastFog = -1.f;
    Apply(Cloud, Fog);
}

void FGamePlatformWeatherAtmosphere::OnLevelRemoved(ULevel* Level, UWorld* RemovedWorld)
{
    if (!Level || RemovedWorld != World.Get() || RemovedWorld->bIsTearingDown) return;
    const float Cloud = FMath::Max(LastCloud, 0.f), Fog = FMath::Max(LastFog, 0.f);
    // 卸载对象仍可能保持有效；先恢复原值，重载时不得把已经压暗的值作为新基线。
    Apply(0.f, 0.f);
    Lights.RemoveAll([Level](const auto& Entry) { return !Entry.Component.IsValid() || Entry.Component->GetOwner()->GetLevel() == Level; });
    Fogs.RemoveAll([Level](const auto& Entry) { return !Entry.Component.IsValid() || Entry.Component->GetOwner()->GetLevel() == Level; });
    EnsureLocalFog();
    LastCloud = LastFog = -1.f;
    Apply(Cloud, Fog);
}

FGamePlatformWeatherAtmosphere::~FGamePlatformWeatherAtmosphere()
{
    check(IsInGameThread());
    FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
    FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
    Apply(0.f, 0.f);
    if (OwnedFog.IsValid()) OwnedFog->Destroy();
}

void FGamePlatformWeatherAtmosphere::Apply(float Cloud, float Fog)
{
    check(IsInGameThread());
    if (!Profile.bEnabled || !World.IsValid() || World->GetNetMode() == NM_DedicatedServer ||
        !FMath::IsFinite(Cloud) || !FMath::IsFinite(Fog)) return;
    Cloud = FMath::Clamp(Cloud, 0.f, 1.f); Fog = FMath::Clamp(Fog, 0.f, 1.f);
    if (FMath::IsNearlyEqual(Cloud, LastCloud) && FMath::IsNearlyEqual(Fog, LastFog)) return;
    LastCloud = Cloud; LastFog = Fog;
    const FLinearColor Tint = FMath::Lerp(FLinearColor::White, Profile.LightTint, Cloud);
    for (const auto& Entry : Lights)
    {
        auto* Light = Entry.Component.Get(); if (!IsValid(Light)) continue;
        const float Multiplier = FMath::Clamp(Entry.bSky ? Profile.OvercastSkyMultiplier : Profile.OvercastSunMultiplier, 0.f, 1.f);
        const float Intensity = Entry.Intensity * FMath::Lerp(1.f, Multiplier, Cloud);
        if (auto* Sky = Cast<USkyLightComponent>(Light))
        { Sky->SetIntensity(Intensity); Sky->SetLightColor(Entry.Color * Tint); }
        else if (auto* Sun = Cast<UDirectionalLightComponent>(Light))
        { Sun->SetIntensity(Intensity); Sun->SetLightColor(Entry.Color * Tint); }
    }
    for (const auto& Entry : Fogs)
    {
        auto* Component = Entry.Component.Get(); if (!IsValid(Component)) continue;
        Component->SetFogDensity(Entry.Density + FMath::Clamp(Profile.AdditionalFogDensity, 0.f, .1f) * Fog);
        Component->SetStartDistance(FMath::Lerp(Entry.Start, FMath::Min(Entry.Start,
            FMath::Max(Profile.FogStartDistanceCm, 0.f)), Fog));
        Component->SetFogInscatteringColor(FMath::Lerp(Entry.Color, Profile.FogColor, Fog));
    }
    // 有变化的审核采样，不扫描世界；真实组件值与目标权重同时记录，不能只以请求受理冒充生效。
    if (FMath::Abs(Cloud - LastLoggedCloud) >= .15f || FMath::Abs(Fog - LastLoggedFog) >= .15f ||
        ((Cloud == 0.f && Fog == 0.f) && (LastLoggedCloud != 0.f || LastLoggedFog != 0.f)))
    {
        float Sun = -1.f, Sky = -1.f, Density = -1.f;
        for (const auto& Entry : Lights) if (Entry.Component.IsValid())
        { if (Entry.bSky) Sky = Entry.Component->Intensity; else Sun = Entry.Component->Intensity; }
        for (const auto& Entry : Fogs) if (Entry.Component.IsValid()) Density = Entry.Component->FogDensity;
        UE_LOG(LogGamePlatformWeatherAtmosphere, Verbose,
            TEXT("GP_WEATHER_ATMOSPHERE cloud=%.3f fog=%.3f sun=%.3f sky=%.3f density=%.5f lights=%d fogs=%d"),
            Cloud, Fog, Sun, Sky, Density, Lights.Num(), Fogs.Num());
        LastLoggedCloud = Cloud; LastLoggedFog = Fog;
    }
}

float FGamePlatformWeatherAtmosphere::CloudWeight(const FGamePlatformWeatherState& State)
{
    if (State.Type == EGamePlatformWeatherType::Cloudy) return .65f;
    if (State.Type == EGamePlatformWeatherType::Thunderstorm || State.Type == EGamePlatformWeatherType::Blizzard) return 1.f;
    return FMath::Clamp(FMath::Max(State.SnowIntensity, State.RainIntensity), 0.f, 1.f);
}
