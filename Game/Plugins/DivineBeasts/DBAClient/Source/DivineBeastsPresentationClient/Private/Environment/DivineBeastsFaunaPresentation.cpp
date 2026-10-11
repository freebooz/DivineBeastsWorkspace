// 神兽联盟蝴蝶/鱼群项目入口：目录/逻辑ID属于项目；加载与播放复用Data/Presentation/唯一VFX。
// 游戏线程受理Desired。Count0撤销；Count1..512、Seed0..1000000，真资源Active后才提交。
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

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


bool UDivineBeastsPresentationClientSubsystem::ResolveAmbientFaunaContextForOwner(
    FName HabitatId, const UObject* Owner, const FGuid& RegistrationToken,
    FName& OutContextId, FString& OutError) const
{
    check(IsInGameThread());
    OutContextId = NAME_None;
    OutError.Reset();
    UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
    if (bClosing || bStoppingEnvironmentFlocks || AmbientFauna.bStopping ||
        EnvironmentFlockContextSuspensions > 0 || !IsValid(World) || World->bIsTearingDown ||
        World->GetNetMode() == NM_DedicatedServer || AmbientFauna.World.Get() != World)
    {
        OutError = TEXT("当前群落世界尚未就绪、正在切换或已经退出。");
        return false;
    }
    if (HabitatId.IsNone() || !IsValid(Owner) || Owner->GetWorld() != World || !RegistrationToken.IsValid())
    {
        OutError = TEXT("群落诊断要求当前世界中的存活Owner和有效注册身份。");
        return false;
    }
    const auto* State = AmbientFauna.Requests.Find(HabitatId);
    if (!State || State->Owner.Get() != Owner || State->RegistrationToken != RegistrationToken)
    {
        OutError = TEXT("群落注册不存在或已由另一Owner/注册代次接管。");
        return false;
    }
    if (State->Count <= 0 || !State->RequestId.IsValid())
    {
        OutError = TEXT("该群落尚未提交、已经停止或正等待显式重试。");
        return false;
    }
    OutContextId = HabitatId;
    return true;
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
