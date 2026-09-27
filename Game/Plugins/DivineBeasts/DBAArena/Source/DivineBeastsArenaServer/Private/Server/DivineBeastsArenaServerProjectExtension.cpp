#include "Server/DivineBeastsArenaServerProjectExtension.h"

#include "Catalog/DivineBeastsArenaModeCatalog.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Eligibility/DivineBeastsArenaEligibility.h"
#include "Features/IModularFeatures.h"
#include "Framework/GamePlatformArenaGameMode.h"

bool FDivineBeastsArenaServerProjectExtension::ResolveModeSpec(
    const FGamePlatformArenaAssignment& Assignment,
    FGamePlatformArenaModeSpec& OutModeSpec,
    FString& OutError) const
{
    return FDivineBeastsArenaModeCatalog::ValidateAssignmentAgainstProduction(
        Assignment,
        OutModeSpec,
        OutError);
}

bool FDivineBeastsArenaServerProjectExtension::ValidateAndConfigureGameMode(
    AGamePlatformArenaGameMode& GameMode,
    const FGamePlatformArenaAssignment& Assignment,
    const FGamePlatformArenaModeSpec& ModeSpec,
    FString& OutError)
{
    if (ModeSpec.ArenaModeId != Assignment.ArenaModeId)
    {
        OutError = TEXT("项目Arena ModeSpec与Assignment不一致。");
        return false;
    }

    // HeroSelection必须使用项目可信资格Provider。
    GameMode.SetHeroEligibilityProvider(this);

    // 当前工程尚未提供平台统一Spawn/Respawn实现，严禁项目层自行SpawnActor/Possess绕过。
    if (!GameMode.HasGameplayLifecycleAdapter())
    {
        OutError = TEXT("MainArena GameplayLifecycleAdapter尚未由服务器组合根配置；项目服务器Fail Closed。");
        return false;
    }

    OutError.Reset();
    return true;
}

bool FDivineBeastsArenaServerProjectExtension::IsHeroEligible(
    const FString& PlayerId,
    const FString& HeroDefinitionId,
    FName ArenaModeId,
    FString& OutReason) const
{
    const FName HeroId(*HeroDefinitionId);
    if (PlayerId.IsEmpty() ||
        !FDivineBeastsHeroCatalog::IsCoreHeroId(HeroId) ||
        FDivineBeastsArenaModeCatalog::Find(ArenaModeId) == nullptr)
    {
        OutReason = TEXT("Player/Hero/ArenaMode不在项目允许范围。");
        return false;
    }

    if (!FDivineBeastsHeroCatalog::GetDefinitionAssetPath(HeroId).IsValid())
    {
        OutReason = TEXT("Hero Definition资产未注册，竞技资格Fail Closed。");
        return false;
    }

    const TArray<IDivineBeastsArenaTrustedHeroEligibilityProvider*> Providers =
        IModularFeatures::Get()
            .GetModularFeatureImplementations<IDivineBeastsArenaTrustedHeroEligibilityProvider>(
                IDivineBeastsArenaTrustedHeroEligibilityProvider::GetModularFeatureName());

    if (Providers.Num() != 1 || Providers[0] == nullptr)
    {
        OutReason = TEXT("可信Entitlement/Maintenance Hero资格Provider未唯一配置。");
        return false;
    }

    return Providers[0]->IsHeroEligible(
        PlayerId,
        HeroId,
        ArenaModeId,
        OutReason);
}
