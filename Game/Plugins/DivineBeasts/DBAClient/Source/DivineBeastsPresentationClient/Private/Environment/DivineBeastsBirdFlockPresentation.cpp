// 神兽联盟正式鸟群入口；所有权/异步资源/取消重入与Fauna复用项目内部环境群落事务。
// 仅提交中立请求，无Niagara实例或Tick；原有Blueprint API保持兼容。
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Environment/DivineBeastsBirdFlockRouteActor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

bool UDivineBeastsPresentationClientSubsystem::StartAmbientBirdFlock(
    FName FlockId, FName SpeciesId, int32 BirdCount, const FVector& WorldOrigin, FString& OutError)
{
    return StartEnvironmentFlock(BirdFlocks, FlockId, SpeciesId, BirdCount, 0, WorldOrigin, nullptr, {}, OutError);
}

bool UDivineBeastsPresentationClientSubsystem::StartAmbientBirdFlockForOwner(
    FName FlockId, FName SpeciesId, int32 BirdCount, const FVector& WorldOrigin,
    UObject* Owner, const FGuid& RegistrationToken, FString& OutError)
{
    check(IsInGameThread());
    if (!IsValid(Owner) || !RegistrationToken.IsValid())
    {
        OutError = TEXT("场景鸟群要求存活Owner和有效注册代次。");
        return false;
    }
    return StartEnvironmentFlock(BirdFlocks, FlockId, SpeciesId, BirdCount, 0, WorldOrigin,
        Owner, RegistrationToken, OutError);
}

/** 神兽联盟项目鸟群高级配置仅扩展原Owner安全意图，底层仍使用Presentation/Data/VFX。 */
bool UDivineBeastsPresentationClientSubsystem::StartAmbientBirdFlockForOwnerAdvanced(
    FName FlockId, FName SpeciesId, int32 BirdCount, const FVector& WorldOrigin,
    UObject* Owner, const FGuid& RegistrationToken,
    const FDivineBeastsBirdFlockFlightSettings& FlightSettings, FString& OutError)
{
    check(IsInGameThread());
    if (!IsValid(Owner) || !RegistrationToken.IsValid())
    {
        OutError = TEXT("高级鸟群要求存活Owner和有效注册代次。");
        return false;
    }
    // 真实GPU样条、Niagara参数白名单和项目物种系统尚需同版本生成、回读和验收。
    // 未打开明确的项目发布资格门禁时绝不让模式3..5误回退为现有FigureEight视觉假成功。
    bool bAuthorizedAdvancedBirdFlock = false;
    if (!GConfig || !GConfig->GetBool(TEXT("DivineBeasts.BirdFlock.Advanced"),
            TEXT("bEnableVerifiedGPUFlight"), bAuthorizedAdvancedBirdFlock, GGameIni) ||
        !bAuthorizedAdvancedBirdFlock)
    {
        OutError = TEXT("高级鸟群Niagara资源未完成同版本验收，正式路径暂不受理；旧0/1/2模式不受影响。");
        return false;
    }
    return StartEnvironmentFlock(BirdFlocks, FlockId, SpeciesId, BirdCount, FlightSettings.Seed,
        WorldOrigin, Owner, RegistrationToken, OutError, &FlightSettings);
}

bool UDivineBeastsPresentationClientSubsystem::StopAmbientBirdFlockForOwner(FName FlockId, const FGuid& RegistrationToken)
{
    return StopEnvironmentFlockForOwner(BirdFlocks, FlockId, RegistrationToken);
}

void UDivineBeastsPresentationClientSubsystem::StopAmbientBirdFlock(FName FlockId)
{
    StopEnvironmentFlock(BirdFlocks, FlockId);
}

void UDivineBeastsPresentationClientSubsystem::StopAllAmbientBirdFlocks()
{
    StopAllEnvironmentFlocks(BirdFlocks);
}

int32 UDivineBeastsPresentationClientSubsystem::GetRequestedBirdCount(FName FlockId) const
{
    const auto* State = BirdFlocks.Requests.Find(FlockId);
    return State ? State->Count : 0;
}

void UDivineBeastsPresentationClientSubsystem::StartDefaultWorldBirdFlock(UWorld* World)
{
    if (!World || !GetLocalPlayer() || World != GetLocalPlayer()->GetWorld() || World->GetNetMode() == NM_DedicatedServer) return;
    const FString Package = World->GetOutermost()->GetName();
    const bool bVillage = Package.StartsWith(TEXT("/DBAWorldPack_Village/")) ||
        ProjectContext.ExperienceId.ToString().StartsWith(TEXT("Experience.Village."));
    const bool bOpenWorld = Package.StartsWith(TEXT("/DBAWorldPack_OpenWorld/")) ||
        ProjectContext.WorldId.ToString().StartsWith(TEXT("World.OpenWorld."));
    if (!bVillage && !bOpenWorld) return;
    // 分屏共享环境，只由首个本地玩家自动建立默认群。
    if (!World->GetGameInstance() || World->GetGameInstance()->GetFirstGamePlayer() != GetLocalPlayer()) return;
    // 项目内容地图如已明确放置真实ClientOnly样条群组，则先由其自身Owner/Token启动。
    // 无路线Actor或新GPU内容尚未验收时，原有单群环境鸟作为安全可见回退。
    bool bHavePlayableSplineRoute = false;
    for (TActorIterator<ADivineBeastsBirdFlockRouteActor> It(World); It; ++It)
    {
        if (ADivineBeastsBirdFlockRouteActor* Route = *It)
        {
            FString RouteError;
            if (Route->StartRouteBirds(RouteError)) bHavePlayableSplineRoute = true;
            else UE_LOG(LogTemp, Warning, TEXT("[BirdFlock] 项目生态样条不可用，保留旧路径回退：%s"),
                *RouteError);
        }
    }
    if (bHavePlayableSplineRoute) return;
    FVector Origin(0, 0, 1800);
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        Origin = It->GetActorLocation() + FVector(0, 0, 1800);
        break;
    }
    FString Error;
    if (!StartAmbientBirdFlock(TEXT("DBA.Environment.WorldBirds"), bVillage ? FName(TEXT("Seagull")) : FName(TEXT("Eagle")),
        bVillage ? 24 : 5, Origin, Error))
        UE_LOG(LogTemp, Warning, TEXT("[BirdFlock] 默认世界鸟群尚未受理：%s"), *Error);
}
