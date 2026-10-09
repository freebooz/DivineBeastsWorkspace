// 项目竞技服务器组合路由回归：两个真实World/GameMode独立保留状态，结束其中一个不覆盖另一世界。
#include "Server/DivineBeastsArenaServerProjectExtension.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Server/DivineBeastsArenaGameplayLifecycleAdapter.h"
#include "Framework/GamePlatformArenaPlayerController.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Definitions/GamePlatformArenaBuiltinModes.h"
#include "Characters/DivineBeastsGameplayCharacter.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaWorldOwnershipTest,
    "DivineBeasts.Arena.Server.WorldOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaWorldOwnershipTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UWorld> A(UWorld::CreateWorld(EWorldType::Game, false));
    TStrongObjectPtr<UWorld> B(UWorld::CreateWorld(EWorldType::Game, false));
    FDivineBeastsArenaServerProjectExtension Extension;
    auto* ModeA = A->SpawnActor<AGamePlatformArenaGameMode>();
    auto* ModeB = B->SpawnActor<AGamePlatformArenaGameMode>();
    TestNotNull(TEXT("真实世界A有独立GameMode"), ModeA);
    TestNotNull(TEXT("真实世界B有独立GameMode"), ModeB);
    if (ModeA && ModeB)
    {
        Extension.WorldAssemblies.Add(ModeA);
        Extension.WorldAssemblies.Add(ModeB);
        Extension.ReleaseWorldAssembly(ModeB);
        Extension.ReleaseWorldAssembly(ModeB);
        TestTrue(TEXT("重复结束B保留A所有权"), Extension.WorldAssemblies.Contains(ModeA));
        TestFalse(TEXT("B自身桶撤销"), Extension.WorldAssemblies.Contains(ModeB));
        // 真实WorldCleanup事件应回收A的桶；这里不伪造资源成功，也不宣称比赛出生通过。
        A->DestroyWorld(false);
        TestEqual(TEXT("世界清理及时回收桶"), Extension.WorldAssemblies.Num(), 0);
    }
    if (!ModeA || !ModeB) A->DestroyWorld(false);
    B->DestroyWorld(false);
    return true;
}

namespace
{
enum class ESpawnReentryScenario { ReplaceAdapter, HigherContext, SameContext, EndDuringActive, UnregisterDuringActive };
// 仅用于本回归的可信选英雄端口；批准调用方指定且已真实预热的Hero，不能为不存在的Hero伪造定义成功。
class FSpawnOwnershipHeroEligibility final : public IGamePlatformArenaHeroEligibilityProvider
{
public:
    explicit FSpawnOwnershipHeroEligibility(FString InHero) : Hero(MoveTemp(InHero)) {}
    bool IsHeroEligible(const FString&, const FString& RequestedHero, FName, FString& Reason) const override
    { if (RequestedHero != Hero) { Reason = TEXT("测试受信Hero不匹配"); return false; } Reason.Reset(); return true; }
private:
    FString Hero;
};

// 实际Restart→Possess→native OnNewPawn切换装配并Possess后继；旧请求不得销毁Controller返回时的后继。
class FSpawnReentrantOwnershipCommand final : public IAutomationLatentCommand
{
public:
    FSpawnReentrantOwnershipCommand(FAutomationTestBase* InTest, FName InHero, ESpawnReentryScenario InScenario)
        : Test(InTest), Hero(InHero), Scenario(InScenario), Start(FPlatformTime::Seconds()) {}
    ~FSpawnReentrantOwnershipCommand() override
    {
        if (!Instance.IsValid()) { return; }
        if (Controller.IsValid()) { Controller->GetOnNewPawnNotifier().Remove(NewPawnHandle); }
        if (ObservedIdentity.IsValid()) { ObservedIdentity->OnReadinessChanged().Remove(IdentityHandle); }
        if (ObservedEligibility.IsValid()) { ObservedEligibility->OnEligibilityChanged().Remove(EligibilityHandle); }
        if (Mode.IsValid()) { Mode->SetGameplayLifecycleAdapter(nullptr); Mode->SetHeroEligibilityProvider(nullptr); }
        Adapter.Reset(); SuccessorAdapter.Reset();
        if (auto* Data = IGamePlatformDataService::Get(*Instance))
        { if (WarmupLease.IsValid()) { Data->ReleaseResources(WarmupLease); } }
        UWorld* World = Instance->GetWorld();
        if (World) { World->EndPlay(EEndPlayReason::Quit); }
        Instance->Shutdown();
        if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Start > 30.0) { Test->AddError(TEXT("真实Spawn所有权回归等待Hero超时")); return true; }
        if (!Instance.IsValid())
        {
            Path = FDivineBeastsHeroCatalog::GetDefinitionAssetPath(Hero);
            if (!Path.IsValid()) { Test->AddError(TEXT("Spawn重入回归缺真实注册Hero主资产")); return true; }
            Instance.Reset(NewObject<UGameInstance>(GEngine)); Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
            UWorld* World = Instance->GetWorld(); auto* Data = IGamePlatformDataService::Get(*Instance);
            if (!Test->TestNotNull(TEXT("真实GI世界"), World) || !Test->TestNotNull(TEXT("正式Data服务"), Data)) { return true; }
            FURL URL; URL.AddOption(*FString::Printf(TEXT("game=%s"), *AGamePlatformArenaGameMode::StaticClass()->GetPathName()));
            if (!Test->TestTrue(TEXT("真实竞技GameMode创建"), World->SetGameMode(URL))) { return true; }
            World->InitializeActorsForPlay(URL); World->BeginPlay(); Mode = Cast<AGamePlatformArenaGameMode>(World->GetAuthGameMode());
            if (!Test->TestTrue(TEXT("真实竞技Mode已开始"), Mode.IsValid() && World->HasBegunPlay())) { return true; }
            // 真实装配以GameMode拥有预热需求；后续Adapter交接的弱Owner与实际租约Owner一致。
            WarmupOwner = Mode.Get();
            FGamePlatformResult Accepted;
            WarmupLease = Data->AcquireResources({Path}, EGamePlatformDataLifetime::World, WarmupOwner.Get(),
                [WeakOwner = WarmupOwner](const FGamePlatformDataLease&, const FGamePlatformResult& Result)
                { if (WeakOwner.IsValid() && !Result.IsSuccess()) { UE_LOG(LogTemp, Warning, TEXT("Spawn ownership warmup failed: %s"), *Result.Code.ToString()); } }, Accepted);
            if (!Test->TestTrue(TEXT("真实Spawn资源预热受理"), Accepted.IsSuccess())) { return true; }
            return false;
        }
        UWorld* World = Instance->GetWorld(); auto* Data = IGamePlatformDataService::Get(*Instance);
        if (!World || !Data || !Mode.IsValid()) { Test->AddError(TEXT("Spawn所有权回归世界或服务已退出")); return true; }
        World->Tick(LEVELTICK_All, 0.01f);
        const auto LeaseState = Data->GetLeaseState(WarmupLease);
        if (LeaseState == EGamePlatformDataRequestState::Loading) { return false; }
        if (!Test->TestEqual(TEXT("真实Spawn预热已完成"), LeaseState, EGamePlatformDataRequestState::Succeeded)) { return true; }
        const auto* Definition = Cast<UDivineBeastsHeroDefinition>(Path.ResolveObject());
        if (!Test->TestNotNull(TEXT("真实Hero Definition可读"), Definition)) { return true; }
        const auto* Spec = FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Duel1v1);
        if (!Test->TestNotNull(TEXT("真实内置1v1规格"), Spec)) { return true; }
        if (bWaitingForMatch)
        {
            if (Mode->GetMatchPhase() == EGamePlatformArenaMatchPhase::Countdown) { return false; }
            if (!Test->TestEqual(TEXT("真实阶段事务及两名真实Pawn进入InProgress"), Mode->GetMatchPhase(), EGamePlatformArenaMatchPhase::InProgress)) { return true; }
            return ExerciseSpawn(*World, *Spec, TerminalAssignment);
        }
        FGamePlatformArenaAssignment Assignment;
        Assignment.MatchId = TEXT("Automation.SpawnOwnership.Match"); Assignment.GameServerId = TEXT("Automation.Server");
        Assignment.ArenaModeId = Spec->ArenaModeId; Assignment.MapId = Spec->MapId; Assignment.TeamSize = 1; Assignment.TotalPlayers = 2;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            FGamePlatformArenaRosterSlot Slot; Slot.PlayerId = FString::Printf(TEXT("Spawn.Player.%d"), Index);
            Slot.CharacterId = FString::Printf(TEXT("Spawn.Character.%d"), Index); Slot.TeamId = FName(*FString::Printf(TEXT("Team.%d"), Index));
            Slot.SlotIndex = Index; Assignment.Roster.Add(Slot);
        }
        FString Error;
        if (!Test->TestTrue(TEXT("真实Assignment受理"), Mode->ApplyAssignment(Assignment, Error))) { Test->AddError(Error); return true; }
        TArray<AGamePlatformArenaPlayerState*> States;
        for (const auto& Slot : Assignment.Roster)
        {
            auto* PlayerController = World->SpawnActor<AGamePlatformArenaPlayerController>();
            if (!Test->TestNotNull(TEXT("真实竞技Controller"), PlayerController)) { return true; }
            auto* PlayerState = PlayerController->GetPlayerState<AGamePlatformArenaPlayerState>();
            if (!Test->TestNotNull(TEXT("引擎创建的PlayerState"), PlayerState) ||
                !Test->TestTrue(TEXT("真实PlayerState归属Controller"), PlayerState->GetOwner() == PlayerController)) { return true; }
            FGamePlatformArenaTransferTicketClaims Claims; Claims.PlayerId = Slot.PlayerId; Claims.CharacterId = Slot.CharacterId;
            Claims.MatchId = Assignment.MatchId; Claims.DestinationServerId = Assignment.GameServerId; Claims.bConsumed = true;
            Claims.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromMinutes(5);
            if (!Test->TestTrue(TEXT("测试受信票据准入"), Mode->AdmitPlayer(PlayerState, Claims, Error))) { Test->AddError(Error); return true; }
            States.Add(PlayerState); if (!Controller.IsValid()) { Controller = PlayerController; }
        }
        FSpawnOwnershipHeroEligibility Heroes(Hero.ToString());
        for (auto* PlayerState : States)
        { if (!Test->TestTrue(TEXT("受信玩家实际选择真实Hero"), Mode->RequestHeroSelection(PlayerState, Hero.ToString(), &Heroes, Error))) { Test->AddError(Error); return true; } }
        for (int32 Index = 0; Index < Assignment.Roster.Num(); ++Index)
        {
            auto* PlayerStart = World->SpawnActor<APlayerStart>(FVector(Index * 5000.0f, 0, 0), FRotator::ZeroRotator);
            if (!Test->TestNotNull(TEXT("真实PlayerStart"), PlayerStart)) { return true; }
            PlayerStart->PlayerStartTag = Assignment.Roster[Index].TeamId;
        }
        Adapter = MakeShared<FDivineBeastsArenaGameplayLifecycleAdapter>(*Mode.Get(), TArray<FGamePlatformDataLease>{WarmupLease});
        Mode->SetGameplayLifecycleAdapter(Adapter.Get());
        // Spawn入口只接纳Countdown/InProgress。所有分支真实RequestReady到Countdown，不能停在ReadyCheck假测回调。
        Mode->CountdownSeconds = 0.01f;
        for (auto* PlayerState : States)
        { if (!Test->TestTrue(TEXT("真实玩家Ready请求受理"), Mode->RequestReady(PlayerState, Error))) { Test->AddError(Error); return true; } }
        if (!Test->TestEqual(TEXT("真实阶段事务进入Countdown"), Mode->GetMatchPhase(), EGamePlatformArenaMatchPhase::Countdown)) { return true; }
        if (IsTerminalScenario())
        {
            // 先通过正常真实出生进入比赛，下一实际Automation帧再测局内Active；不改阶段或使用假Spawn成功端口。
            TerminalAssignment = Assignment;
            bWaitingForMatch = true; return false;
        }
        return ExerciseSpawn(*World, *Spec, Assignment);
    }
private:
    bool IsTerminalScenario() const
    { return Scenario == ESpawnReentryScenario::EndDuringActive || Scenario == ESpawnReentryScenario::UnregisterDuringActive; }
    bool ExerciseSpawn(UWorld& World, const FGamePlatformArenaModeSpec& Spec, const FGamePlatformArenaAssignment& Assignment)
    {
        FString Error;
        const FGuid OriginalBinding = Mode->GetGameplayLifecycleBindingId();
        bool bPossessCallback = false;
        NewPawnHandle = Controller->GetOnNewPawnNotifier().AddLambda([&](APawn* NewPawn)
        {
            if (!NewPawn || bPossessCallback) { return; }
            bPossessCallback = true;
            if (Scenario == ESpawnReentryScenario::ReplaceAdapter)
            {
                SuccessorAdapter = MakeShared<FDivineBeastsArenaGameplayLifecycleAdapter>(*Mode.Get(), TArray<FGamePlatformDataLease>{WarmupLease});
                Mode->SetGameplayLifecycleAdapter(SuccessorAdapter.Get());
                FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                SuccessorPawn = World.SpawnActor<ADivineBeastsGameplayCharacter>(FVector(2000, 0, 0), FRotator::ZeroRotator, Spawn);
                if (SuccessorPawn.IsValid()) { Controller->Possess(SuccessorPawn.Get()); }
            }
            else
            {
                // 在本次真实Pawn刚Possess、初始化器尚未执行时安装监听。撤销通知内接管同Pawn，不能只测不同指针。
                SuccessorPawn = Cast<ADivineBeastsGameplayCharacter>(NewPawn);
                ObservedIdentity = NewPawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
                if (!ObservedIdentity.IsValid()) { return; }
                if (IsTerminalScenario())
                {
                    ObservedEligibility = NewPawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
                    if (!ObservedEligibility.IsValid()) { return; }
                    EligibilityHandle = ObservedEligibility->OnEligibilityChanged().AddLambda([this](const FGamePlatformGameplayEligibilitySnapshot& Snapshot)
                    {
                        if (!Snapshot.bActive || bTerminalCallback || !ObservedIdentity.IsValid()) { return; }
                        bTerminalCallback = true; TerminalNonce = ObservedIdentity->GetTrustedContextOperationId();
                        if (Scenario == ESpawnReentryScenario::EndDuringActive) { ObservedIdentity->EndPlay(EEndPlayReason::Destroyed); }
                        else { ObservedIdentity->UnregisterComponent(); }
                    });
                    return;
                }
                IdentityHandle = ObservedIdentity->OnReadinessChanged().AddLambda([this](bool bReady)
                {
                    if (bReady || bContextCallback) { return; } bContextCallback = true;
                    auto* Identity = ObservedIdentity.Get(); auto* PlayerState = Controller->GetPlayerState<AGamePlatformArenaPlayerState>();
                    if (!Identity || !PlayerState) { return; }
                    FGamePlatformCharacterInitializationContext Context; Context.CharacterId = PlayerState->CharacterId;
                    Context.HeroDefinitionId = Hero;
                    Context.SpawnGeneration = Scenario == ESpawnReentryScenario::HigherContext ? 2 : 1;
                    Context.AvatarGeneration = Context.SpawnGeneration; Context.bPersistentCharacterIdRequired = true;
                    FString ContextError;
                    bContextAccepted = Identity->AuthorityBindTrustedContext(Context, ContextError);
                    bContextReady = Identity->TryUsePreloadedDefinition(WarmupLease, *Mode.Get(), ContextError);
                });
            }
        });
        const FName Policy = IsTerminalScenario() ? Spec.RespawnPolicyId : Spec.SpawnPolicyId;
        const bool bOldSpawnSucceeded = Adapter->SpawnPlayer(Assignment.Roster[0].PlayerId, Assignment.Roster[0].TeamId, Policy, Error);
        Controller->GetOnNewPawnNotifier().Remove(NewPawnHandle); NewPawnHandle.Reset();
        if (ObservedIdentity.IsValid()) { ObservedIdentity->OnReadinessChanged().Remove(IdentityHandle); } IdentityHandle.Reset();
        if (ObservedEligibility.IsValid()) { ObservedEligibility->OnEligibilityChanged().Remove(EligibilityHandle); } EligibilityHandle.Reset();
        Test->TestTrue(TEXT("真实Restart/Possess通知已到达"), bPossessCallback);
        Test->TestFalse(TEXT("原装配的出生请求已撤销"), bOldSpawnSucceeded);
        if (Scenario == ESpawnReentryScenario::ReplaceAdapter)
        { Test->TestTrue(TEXT("后继装配已接管"), Mode->GetGameplayLifecycleBindingId() != OriginalBinding); }
        else if (IsTerminalScenario())
        {
            Test->TestTrue(TEXT("真实Active广播触发终态监听"), bTerminalCallback);
            Test->TestTrue(TEXT("关闭没有伪造Context换代"), ObservedIdentity.IsValid() && TerminalNonce.IsValid() && ObservedIdentity->GetTrustedContextOperationId() == TerminalNonce);
            Test->TestTrue(TEXT("终态不靠更换Adapter"), Mode->GetGameplayLifecycleBindingId() == OriginalBinding);
            if (ObservedIdentity.IsValid())
            {
                Test->TestFalse(TEXT("终态公开Ready统一失败关闭"), ObservedIdentity->IsCharacterReady());
                if (Scenario == ESpawnReentryScenario::EndDuringActive)
                { Test->TestFalse(TEXT("真实EndPlay已结束组件"), ObservedIdentity->HasBegunPlay()); }
                else { Test->TestFalse(TEXT("真实Unregister已撤销注册"), ObservedIdentity->IsRegistered()); }
            }
            if (SuccessorPawn.IsValid())
            {
                auto* CharacterASC = SuccessorPawn->GetGamePlatformAbilitySystemComponent();
                if (Test->TestNotNull(TEXT("终态真实ASC仍可读取"), CharacterASC))
                { Test->TestFalse(TEXT("终态能力激活门禁拒绝"), CharacterASC->EvaluateActivationEligibility().IsSuccess()); }
            }
        }
        else
        {
            Test->TestTrue(TEXT("同Pawn真实身份通知已到达"), bContextCallback);
            Test->TestTrue(TEXT("同Pawn后继操作真实受理"), bContextAccepted);
            Test->TestTrue(TEXT("同Pawn后继使用真实预热Ready"), bContextReady);
            Test->TestTrue(TEXT("同Adapter未切换也必须识别后继"), Mode->GetGameplayLifecycleBindingId() == OriginalBinding);
            if (ObservedIdentity.IsValid())
            {
                Test->TestEqual(TEXT("后继身份代次保持"), ObservedIdentity->GetAvatarGeneration(), Scenario == ESpawnReentryScenario::HigherContext ? 2 : 1);
                Test->TestTrue(TEXT("原失败清理不撤销同Pawn后继Ready"), ObservedIdentity->IsCharacterReady());
            }
        }
        Test->TestTrue(TEXT("旧栈没有销毁真实后继Pawn"), SuccessorPawn.IsValid() && !SuccessorPawn->IsActorBeingDestroyed());
        Test->TestTrue(TEXT("真实Controller仍拥有后继"), SuccessorPawn.IsValid() && Controller->GetPawn() == SuccessorPawn.Get() && SuccessorPawn->GetController() == Controller.Get());
        return true;
    }
    FAutomationTestBase* Test;
    FName Hero;
    ESpawnReentryScenario Scenario;
    double Start;
    FSoftObjectPath Path;
    FGamePlatformDataLease WarmupLease;
    TStrongObjectPtr<UGameInstance> Instance;
    TWeakObjectPtr<AActor> WarmupOwner;
    TWeakObjectPtr<AGamePlatformArenaGameMode> Mode;
    TWeakObjectPtr<AGamePlatformArenaPlayerController> Controller;
    TWeakObjectPtr<ADivineBeastsGameplayCharacter> SuccessorPawn;
    FDelegateHandle NewPawnHandle;
    TWeakObjectPtr<UDivineBeastsCharacterComponent> ObservedIdentity;
    FDelegateHandle IdentityHandle;
    bool bContextCallback = false;
    bool bContextAccepted = false;
    bool bContextReady = false;
    TWeakObjectPtr<UGamePlatformGameplayEligibilityComponent> ObservedEligibility;
    FDelegateHandle EligibilityHandle;
    FGuid TerminalNonce;
    bool bTerminalCallback = false;
    bool bWaitingForMatch = false;
    FGamePlatformArenaAssignment TerminalAssignment;
    TSharedPtr<FDivineBeastsArenaGameplayLifecycleAdapter> Adapter;
    TSharedPtr<FDivineBeastsArenaGameplayLifecycleAdapter> SuccessorAdapter;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaSpawnReentrantOwnershipTest,
    "DivineBeasts.Arena.Server.SpawnReentrantSuccessorOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaSpawnReentrantOwnershipTest::RunTest(const FString& Parameters)
{
    (void)Parameters; FString Hero;
    if (!FParse::Value(FCommandLine::Get(), TEXT("DivineBeastsCharacterTestHero="), Hero))
    { AddError(TEXT("Spawn重入回归须指定真实已注册DivineBeastsCharacterTestHero")); return false; }
    // 同一Automation注册独立覆盖真实重入/结束/撤销注册，不把分支数量当成额外通过用例。
    ADD_LATENT_AUTOMATION_COMMAND(FSpawnReentrantOwnershipCommand(this, FName(*Hero), ESpawnReentryScenario::ReplaceAdapter));
    ADD_LATENT_AUTOMATION_COMMAND(FSpawnReentrantOwnershipCommand(this, FName(*Hero), ESpawnReentryScenario::HigherContext));
    ADD_LATENT_AUTOMATION_COMMAND(FSpawnReentrantOwnershipCommand(this, FName(*Hero), ESpawnReentryScenario::SameContext));
    ADD_LATENT_AUTOMATION_COMMAND(FSpawnReentrantOwnershipCommand(this, FName(*Hero), ESpawnReentryScenario::EndDuringActive));
    ADD_LATENT_AUTOMATION_COMMAND(FSpawnReentrantOwnershipCommand(this, FName(*Hero), ESpawnReentryScenario::UnregisterDuringActive)); return true;
}
#endif
