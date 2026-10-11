// 神兽联盟蝴蝶/鱼群项目入口：目录/逻辑ID属于项目；加载与播放复用Data/Presentation/唯一VFX。
// 游戏线程受理Desired。Count0撤销；Count1..512、Seed0..1000000，真资源Active后才提交。
#include "DivineBeastsPresentationClientSubsystem.h"

bool UDivineBeastsPresentationClientSubsystem::StartAmbientFauna(
    FName HabitatId, FName SpeciesId, int32 Count, int32 Seed, const FVector& WorldOrigin, FString& OutError)
{
    return StartEnvironmentFlock(AmbientFauna, HabitatId, SpeciesId, Count, Seed, WorldOrigin, nullptr, {}, OutError);
}

bool UDivineBeastsPresentationClientSubsystem::StartAmbientFaunaForOwner(
    FName HabitatId, FName SpeciesId, int32 Count, int32 Seed, const FVector& WorldOrigin,
    UObject* Owner, const FGuid& RegistrationToken, FString& OutError)
{
    check(IsInGameThread());
    if (!IsValid(Owner) || !RegistrationToken.IsValid())
    {
        OutError = TEXT("场景群落要求存活Owner和有效注册代次。");
        return false;
    }
    return StartEnvironmentFlock(AmbientFauna, HabitatId, SpeciesId, Count, Seed, WorldOrigin,
        Owner, RegistrationToken, OutError);
}

bool UDivineBeastsPresentationClientSubsystem::StopAmbientFaunaForOwner(FName HabitatId, const FGuid& RegistrationToken)
{
    return StopEnvironmentFlockForOwner(AmbientFauna, HabitatId, RegistrationToken);
}

void UDivineBeastsPresentationClientSubsystem::StopAmbientFauna(FName HabitatId)
{
    StopEnvironmentFlock(AmbientFauna, HabitatId);
}

void UDivineBeastsPresentationClientSubsystem::StopAllAmbientFauna()
{
    StopAllEnvironmentFlocks(AmbientFauna);
}

int32 UDivineBeastsPresentationClientSubsystem::GetRequestedFaunaCount(FName HabitatId) const
{
    const auto* State = AmbientFauna.Requests.Find(HabitatId);
    return State ? State->Count : 0;
}
