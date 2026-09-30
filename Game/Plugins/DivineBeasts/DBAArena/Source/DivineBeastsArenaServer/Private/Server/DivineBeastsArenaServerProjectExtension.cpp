// 项目竞技服务器组合根：资格来自可信Provider，预热资源租约属当前GameMode世界，平台Data拥有加载。
#include "Server/DivineBeastsArenaServerProjectExtension.h"

#include "Server/DivineBeastsArenaGameplayLifecycleAdapter.h"

#include "Catalog/DivineBeastsArenaModeCatalog.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Eligibility/DivineBeastsArenaEligibility.h"
#include "Features/IModularFeatures.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "GameFramework/Character.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/IGamePlatformDataService.h"

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

    // MainArena在真正开赛前统一通过生命周期适配器出生；默认PawnClass保持为空，
    // 防止玩家连接后被AGameModeBase提前生成一个尚未初始化的Pawn。
    GameMode.DefaultPawnClass = nullptr;

    // Assignment阶段预热12个很小的Server-safe Hero Definition。
    // 比赛倒计时结束后的Spawn路径严禁冷加载；若预热尚未完成，适配器会Fail Closed。
    ReleaseWorldAssembly(&GameMode);
    UGameInstance* Instance = GameMode.GetGameInstance();
    if (!Instance)
    { OutError = TEXT("竞技预热缺少当前GameInstance。"); return false; }
    FWorldAssembly& Assembly = WorldAssemblies.Add(&GameMode);
    Assembly.Instance = Instance;
    for (const FName HeroId : FDivineBeastsHeroCatalog::GetCoreHeroIds())
    {
        FGamePlatformResult Accepted;
        const FGamePlatformDataLease Lease =
            FDivineBeastsHeroCatalog::AcquireDefinitionResources(*Instance, HeroId,
                EGamePlatformDataLifetime::World, &GameMode,
                [WeakMode = TWeakObjectPtr<AGamePlatformArenaGameMode>(&GameMode), HeroId]
                (UDivineBeastsHeroDefinition* Definition, const FGamePlatformDataLease&, const FGamePlatformResult& Result)
                {
                    // 只报告当前存活世界的预热失败；真正出生仍需适配器核对实际Definition就绪。
                    if (WeakMode.IsValid() && !Result.IsSuccess())
                        UE_LOG(LogTemp, Warning, TEXT("Hero warmup failed: %s (%s)"), *HeroId.ToString(), *Result.Code.ToString());
                }, Accepted);
        if (!Accepted.IsSuccess())
        { OutError = TEXT("竞技英雄预热未被统一数据服务接纳。"); ReleaseWorldAssembly(&GameMode); return false; }
        if (Lease.IsValid())
        {
            Assembly.HeroDefinitionWarmupLeases.Add(Lease);
        }
    }

    Assembly.GameplayLifecycleAdapter =
        MakeShared<FDivineBeastsArenaGameplayLifecycleAdapter>(GameMode);
    // 旧桶释放会撤销借用指针；新组合完整接纳后才重新发布资格与生命周期接口。
    GameMode.SetHeroEligibilityProvider(this);
    GameMode.SetGameplayLifecycleAdapter(Assembly.GameplayLifecycleAdapter.Get());

    OutError.Reset();
    return true;
}

FDivineBeastsArenaServerProjectExtension::FDivineBeastsArenaServerProjectExtension()
{
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(this,
        &FDivineBeastsArenaServerProjectExtension::HandleWorldCleanup);
}
FDivineBeastsArenaServerProjectExtension::~FDivineBeastsArenaServerProjectExtension()
{
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
    TArray<TWeakObjectPtr<AGamePlatformArenaGameMode>> Modes;
    WorldAssemblies.GetKeys(Modes);
    for (const auto& Mode : Modes) ReleaseWorldAssembly(Mode);
}
void FDivineBeastsArenaServerProjectExtension::ReleaseWorldAssembly(TWeakObjectPtr<AGamePlatformArenaGameMode> Mode)
{
    check(IsInGameThread());
    FWorldAssembly Assembly;
    if (!WorldAssemblies.RemoveAndCopyValue(Mode, Assembly)) return;
    // 先撤销GameMode借用的接口，Adapter析构取消自己持有的定时器，再结束本世界资源需求。
    if (Mode.IsValid())
    {
        Mode->SetGameplayLifecycleAdapter(nullptr);
        Mode->SetHeroEligibilityProvider(nullptr);
    }
    Assembly.GameplayLifecycleAdapter.Reset();
    if (auto* Instance = Assembly.Instance.Get())
        if (auto* Data = IGamePlatformDataService::Get(*Instance))
            for (const auto& Lease : Assembly.HeroDefinitionWarmupLeases) Data->ReleaseResources(Lease);
}
void FDivineBeastsArenaServerProjectExtension::HandleWorldCleanup(UWorld* World, bool, bool)
{
    TArray<TWeakObjectPtr<AGamePlatformArenaGameMode>> Modes;
    WorldAssemblies.GetKeys(Modes);
    for (const auto& Mode : Modes)
        if (!Mode.IsValid() || Mode->GetWorld() == World) ReleaseWorldAssembly(Mode);
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
