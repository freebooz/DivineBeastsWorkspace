// 使用真实竞技GameMode和世界定时器，只替换HTTP边界以控制回执丢失、重复和迟到。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Server/GamePlatformArenaServerCoordinator.h"

#include "CoreGlobals.h"
#include "Definitions/GamePlatformArenaBuiltinModes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "TimerManager.h"

namespace
{
using ESubmitStatus = EGamePlatformArenaSubmitStatus;

class FSettlementTransportFixture final : public FGamePlatformArenaServerBackendClient
{
public:
    FSettlementTransportFixture() : FGamePlatformArenaServerBackendClient(FGamePlatformArenaServerBackendConfig{}) {}

    void SubmitMatchResult(const FGamePlatformArenaMatchResult& Result,
        TFunction<void(ESubmitStatus, const FString&)> Completion) override
    {
        Submissions.Add(Result);
        Responses.Add(MoveTemp(Completion));
    }

    void Respond(int32 AttemptIndex, ESubmitStatus Status)
    {
        const auto Completion = Responses[AttemptIndex];
        Completion(Status, FString());
    }

    TArray<FGamePlatformArenaMatchResult> Submissions;
    TArray<TFunction<void(ESubmitStatus, const FString&)>> Responses;
};

class FSettlementGameplayFixture final : public IGamePlatformArenaGameplayLifecycleAdapter,
    public IGamePlatformArenaHeroEligibilityProvider
{
public:
    bool CapturePlayerGameplayOwnership(const FString&, FGamePlatformArenaGameplayOwnership& Out) const override
    { Out = {}; return false; }
    bool SpawnPlayer(const FString&, FName, FName, FString&) override { return true; }
    bool SetPlayerGameplayActive(const FString&, bool, FString&) override { return true; }
    bool RequestRespawn(const FString&, FName, FName, float, FString&) override { return false; }
    bool IsHeroEligible(const FString&, const FString& Hero, FName, FString&) const override
    { return Hero == TEXT("Test.Hero"); }
};

struct FSettlementWorldFixture
{
    ~FSettlementWorldFixture()
    {
        if (World)
        {
            if (Mode)
            {
                Mode->SetGameplayLifecycleAdapter(nullptr);
                Mode->SetHeroEligibilityProvider(nullptr);
            }
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
        }
    }

    bool SetUp(const FString& MatchId, FString& Error)
    {
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
        if (!World) { Error = TEXT("无法创建结算测试世界"); return false; }
        World->SetGameInstance(NewObject<UGameInstance>(World));
        World->GetWorldSettings()->DefaultGameMode = AGamePlatformArenaGameMode::StaticClass();
        if (!World->SetGameMode(FURL())) { Error = TEXT("无法装配权威竞技GameMode"); return false; }
        World->InitializeActorsForPlay(FURL());
        Mode = World->GetAuthGameMode<AGamePlatformArenaGameMode>();
        if (!Mode) { Error = TEXT("缺少真实权威竞技GameMode"); return false; }
        Mode->DispatchBeginPlay();
        Mode->CountdownSeconds = 0.01f;
        Mode->SetGameplayLifecycleAdapter(&Gameplay);
        Mode->SetHeroEligibilityProvider(&Gameplay);

        auto Spec = *FGamePlatformArenaBuiltinModes::Find(FGamePlatformArenaBuiltinModes::Duel1v1);
        Spec.SelectionPolicyId = TEXT("Arena.Selection.LockedRoster");
        FGamePlatformArenaAssignment Assignment;
        Assignment.MatchId = MatchId;
        Assignment.GameServerId = TEXT("Test.Server");
        Assignment.ArenaModeId = Spec.ArenaModeId;
        Assignment.MapId = Spec.MapId;
        Assignment.TeamSize = Spec.TeamSize;
        Assignment.TotalPlayers = Spec.TotalPlayers;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            auto& Slot = Assignment.Roster.AddDefaulted_GetRef();
            Slot.PlayerId = FString::Printf(TEXT("Player.%d"), Index);
            Slot.CharacterId = FString::Printf(TEXT("Character.%d"), Index);
            Slot.TeamId = Index == 0 ? TEXT("team-a") : TEXT("team-b");
            Slot.HeroDefinitionId = TEXT("Test.Hero");
            Slot.SlotIndex = Index;
        }
        if (!Mode->ApplyAssignmentWithModeSpec(Assignment, Spec, Error)) { return false; }
        for (const auto& Slot : Assignment.Roster)
        {
            FGamePlatformArenaTransferTicketClaims Claims;
            Claims.PlayerId = Slot.PlayerId;
            Claims.CharacterId = Slot.CharacterId;
            Claims.MatchId = Assignment.MatchId;
            Claims.DestinationServerId = Assignment.GameServerId;
            Claims.ExpiresAtUtc = FDateTime::UtcNow() + FTimespan::FromMinutes(1);
            Claims.bConsumed = true;
            if (!Mode->AdmitPlayer(World->SpawnActor<AGamePlatformArenaPlayerState>(), Claims, Error)) { return false; }
        }
        return true;
    }

    void Submit()
    {
        Coordinator->SubmitPendingResult(Mode, [this](bool bCommitted, const FString&)
        {
            ++CompletionCount;
            bWasCommitted = bCommitted;
        });
    }

    FSettlementGameplayFixture Gameplay;
    UWorld* World = nullptr;
    AGamePlatformArenaGameMode* Mode = nullptr;
    TSharedRef<FSettlementTransportFixture> Transport = MakeShared<FSettlementTransportFixture>();
    TSharedRef<FGamePlatformArenaServerCoordinator> Coordinator = MakeShared<FGamePlatformArenaServerCoordinator>(Transport);
    int32 CompletionCount = 0;
    bool bWasCommitted = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformArenaSettlementRetryTest,
    "GamePlatform.Arena.Server.SettlementRetriesFrozenResultUntilConfirmed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformArenaSettlementRetryTest::RunTest(const FString&)
{
    const auto Recovery = MakeShared<FSettlementWorldFixture>();
    const auto Conflict = MakeShared<FSettlementWorldFixture>();
    const auto Closing = MakeShared<FSettlementWorldFixture>();
    const auto InFlight = MakeShared<FSettlementWorldFixture>();
    const TArray<TSharedRef<FSettlementWorldFixture>> Worlds = {Recovery, Conflict, Closing, InFlight};
    for (int32 Index = 0; Index < Worlds.Num(); ++Index)
    {
        FString Error;
        if (!Worlds[Index]->SetUp(FString::Printf(TEXT("Settlement.%d"), Index), Error))
        {
            AddError(Error);
            return false;
        }
    }
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
        [this, Worlds, Recovery, Conflict, Closing, InFlight, Stage = 0, LastFrame = MAX_uint64]() mutable
    {
        if (LastFrame == GFrameCounter) { return false; }
        LastFrame = GFrameCounter;
        const float Delta = Stage == 2 ? 0.25f : Stage >= 4 ? 33.0f : 1.0f;
        for (const auto& Fixture : Worlds) { Fixture->World->GetTimerManager().Tick(Delta); }
        switch (Stage++)
        {
        case 0:
            return false;
        case 1:
            for (const auto& Fixture : Worlds)
            {
                FString Error;
                if (!Fixture->Mode->EndMatch(TEXT("team-a"), EGamePlatformArenaMatchEndReason::ScoreLimit, Error))
                {
                    AddError(Error);
                    return true;
                }
                Fixture->Submit();
                if (!TestEqual(TEXT("每个世界立即提交一次冻结结果"), Fixture->Transport->Submissions.Num(), 1))
                { return true; }
            }
            Recovery->Submit();
            TestEqual(TEXT("重复通知共用已在途序列"), Recovery->Transport->Submissions.Num(), 1);
            Recovery->Transport->Respond(0, ESubmitStatus::RetryableFailure);
            TestEqual(TEXT("回执丢失时不得立即连发"), Recovery->Transport->Submissions.Num(), 1);
            Recovery->Transport->Respond(0, ESubmitStatus::Committed);
            TestEqual(TEXT("已结束尝试的迟到回执不推进比赛"), Recovery->Mode->GetMatchPhase(),
                EGamePlatformArenaMatchPhase::ResultPending);
            Conflict->Transport->Respond(0, ESubmitStatus::Conflict);
            Conflict->Submit();
            Closing->Transport->Respond(0, ESubmitStatus::RetryableFailure);
            Closing->Coordinator->CancelPendingResult();
            Closing->Submit();
            InFlight->Coordinator->CancelPendingResult();
            InFlight->Transport->Respond(0, ESubmitStatus::AlreadyCommitted);
            return false;
        case 2:
            TestEqual(TEXT("退避期内仍只有原始请求"), Recovery->Transport->Submissions.Num(), 1);
            return false;
        case 3:
        case 4:
        case 5:
            if (!TestEqual(TEXT("退避到期只重放一次"), Recovery->Transport->Submissions.Num(), Stage - 2))
            { return true; }
            Recovery->Transport->Respond(Stage - 3, ESubmitStatus::RetryableFailure);
            return false;
        default:
            if (!TestEqual(TEXT("长故障后仍可重放并确认"), Recovery->Transport->Submissions.Num(), 5))
            { return true; }
            const auto& Original = Recovery->Transport->Submissions[0];
            TestEqual(TEXT("原比赛身份"), Original.MatchId, FString(TEXT("Settlement.0")));
            TestEqual(TEXT("原终局修订"), Original.EndRevision, 1);
            for (const auto& Replay : Recovery->Transport->Submissions)
            {
                TestTrue(TEXT("重放含同一时间、比分和名单"), FGamePlatformArenaMatchResult::StaticStruct()
                    ->CompareScriptStruct(&Original, &Replay, 0));
            }
            Recovery->Transport->Respond(4, ESubmitStatus::AlreadyCommitted);
            Recovery->Transport->Respond(4, ESubmitStatus::Committed);
            TestEqual(TEXT("幂等已提交回执使真实GameMode正式完成"), Recovery->Mode->GetMatchPhase(),
                EGamePlatformArenaMatchPhase::Completed);
            TestEqual(TEXT("只发出一次完成通知"), Recovery->CompletionCount, 1);
            TestTrue(TEXT("完成通知确认已提交"), Recovery->bWasCommitted);
            TestEqual(TEXT("真正冲突仅完成一次失败且不重试"), Conflict->CompletionCount, 1);
            TestEqual(TEXT("真正冲突只有一次请求"), Conflict->Transport->Submissions.Num(), 1);
            TestFalse(TEXT("冲突禁止进入排空"), Conflict->bWasCommitted);
            TestEqual(TEXT("关闭世界清理退避定时器"), Closing->Transport->Submissions.Num(), 1);
            TestEqual(TEXT("取消等待不发送结算终态"), Closing->CompletionCount, 0);
            TestEqual(TEXT("关闭后的在途回执不发送结算终态"), InFlight->CompletionCount, 0);
            TestEqual(TEXT("关闭世界的迟到回执不推进比赛"), InFlight->Mode->GetMatchPhase(),
                EGamePlatformArenaMatchPhase::ResultPending);
            return true;
        }
    }));
    return true;
}
#endif
