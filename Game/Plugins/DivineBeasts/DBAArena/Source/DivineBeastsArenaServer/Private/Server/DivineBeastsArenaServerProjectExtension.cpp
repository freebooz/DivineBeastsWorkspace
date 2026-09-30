#include "Server/DivineBeastsArenaServerProjectExtension.h"

#include "Server/DivineBeastsArenaGameplayLifecycleAdapter.h"

#include "Catalog/DivineBeastsArenaModeCatalog.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Eligibility/DivineBeastsArenaEligibility.h"
#include "Features/IModularFeatures.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "GameFramework/Character.h"
#include "Engine/StreamableManager.h"

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

    // MainArena在真正开赛前统一通过生命周期适配器出生；默认PawnClass保持为空，
    // 防止玩家连接后被AGameModeBase提前生成一个尚未初始化的Pawn。
    GameMode.DefaultPawnClass = nullptr;

    // Assignment阶段预热12个很小的Server-safe Hero Definition。
    // 比赛倒计时结束后的Spawn路径严禁冷加载；若预热尚未完成，适配器会Fail Closed。
    HeroDefinitionWarmupLeases.Reset();
    for (const FName HeroId : FDivineBeastsHeroCatalog::GetCoreHeroIds())
    {
        TSharedPtr<FStreamableHandle> Lease =
            FDivineBeastsHeroCatalog::RequestDefinition(
                HeroId,
                [](UDivineBeastsHeroDefinition*) {});
        if (Lease.IsValid())
        {
            HeroDefinitionWarmupLeases.Add(MoveTemp(Lease));
        }
    }

    GameplayLifecycleAdapter =
        MakeShared<FDivineBeastsArenaGameplayLifecycleAdapter>(GameMode);
    GameMode.SetGameplayLifecycleAdapter(GameplayLifecycleAdapter.Get());

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
