// 本文件用真实GameMode/Controller/PlayerState/Pawn验证五模式淘汰事务；端口夹具仅替代资源出生，不冒充GAS/网络验签。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Framework/GamePlatformArenaPlayerController.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Definitions/GamePlatformArenaBuiltinModes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

namespace
{
/** 测试世界拥有真实Pawn；不加载项目资源，故只声明验证规则调用次数和原实体所有权。 */
class FEliminationLifecycleFixture final:public IGamePlatformArenaGameplayLifecycleAdapter
{
public:
    UWorld* World=nullptr;
    TMap<FString,AGamePlatformArenaPlayerController*> Controllers;
    TMap<FString,APawn*> Pawns;
    int32 Spawns=0,Respawns=0,Restores=0,InactiveCalls=0;
    bool CapturePlayerGameplayOwnership(const FString& Player,FGamePlatformArenaGameplayOwnership& Out)const override
    {Out={};if(auto* Pawn=Pawns.FindRef(Player)){Out.Pawn=Pawn;Out.AvatarGeneration=1;return true;}return false;}
    bool SpawnPlayer(const FString& Player,FName,FName,FString& Error)override
    {
        auto* Controller=Controllers.FindRef(Player);if(!World||!Controller){Error=TEXT("测试控制器缺失");return false;}
        auto* Pawn=World->SpawnActor<APawn>();if(!Pawn){Error=TEXT("测试Pawn创建失败");return false;}
        Controller->Possess(Pawn);Pawns.Add(Player,Pawn);++Spawns;Error.Reset();return true;
    }
    bool SetPlayerGameplayActive(const FString& Player,bool bActive,FString& Error)override
    {if(!bActive)++InactiveCalls;Error.Reset();return Pawns.Contains(Player);}
    bool RequestRespawn(const FString&,FName,FName,float,FString& Error)override
    {++Respawns;Error=TEXT("单局测试禁止复活");return false;}
    bool RestorePlayerConnection(const FString& Player,FString& Error)override
    {auto* Controller=Controllers.FindRef(Player);auto* Pawn=Pawns.FindRef(Player);if(!Controller||!Pawn){Error=TEXT("原实体缺失");return false;}Controller->Possess(Pawn);++Restores;Error.Reset();return true;}
};
class FEliminationHeroFixture final:public IGamePlatformArenaHeroEligibilityProvider
{
public:bool IsHeroEligible(const FString&,const FString& Hero,FName,FString& Error)const override{Error.Reset();return Hero==TEXT("Test.LockedHero");}
};
struct FEliminationWorldOwner
{
    UWorld* World=nullptr;AGamePlatformArenaGameMode* Mode=nullptr;FEliminationLifecycleFixture Lifecycle;FEliminationHeroFixture Heroes;
    ~FEliminationWorldOwner(){if(Mode){Mode->SetGameplayLifecycleAdapter(nullptr);Mode->SetHeroEligibilityProvider(nullptr);}if(World){World->EndPlay(EEndPlayReason::Quit);World->DestroyWorld(false);}}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformArenaEliminationTest,"GamePlatform.Arena.Lifecycle.FiveModeSingleRoundElimination",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformArenaEliminationTest::RunTest(const FString&)
{
    for(const auto& Builtin:FGamePlatformArenaBuiltinModes::GetAll())
    {
        for(bool bDoubleWipe:{false,true})
        {
            FEliminationWorldOwner Owned;
            const auto Initialization=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
            Owned.World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Initialization);
            if(!TestNotNull(TEXT("淘汰测试真实世界"),Owned.World))return false;
            Owned.World->InitializeActorsForPlay(FURL());Owned.Mode=Owned.World->SpawnActor<AGamePlatformArenaGameMode>();
            if(!TestNotNull(TEXT("真实权威GameMode"),Owned.Mode))return false;
            auto* Mode=Owned.Mode;Mode->DispatchBeginPlay();Owned.Lifecycle.World=Owned.World;Mode->SetGameplayLifecycleAdapter(&Owned.Lifecycle);Mode->SetHeroEligibilityProvider(&Owned.Heroes);
            auto Spec=Builtin;Spec.SelectionPolicyId=TEXT("Arena.Selection.LockedRoster");Spec.RespawnPolicyId=TEXT("Arena.Respawn.Disabled");Spec.WinConditionPolicyId=TEXT("Arena.WinCondition.SingleRoundElimination");Spec.TimeLimitSeconds=1200;
            FGamePlatformArenaAssignment Assignment;Assignment.MatchId=TEXT("Test.Elimination");Assignment.GameServerId=TEXT("Test.Server");Assignment.ArenaModeId=Spec.ArenaModeId;Assignment.MapId=Spec.MapId;Assignment.TeamSize=Spec.TeamSize;Assignment.TotalPlayers=Spec.TotalPlayers;
            for(int32 Index=0;Index<Spec.TotalPlayers;++Index){FGamePlatformArenaRosterSlot Slot;Slot.PlayerId=FString::Printf(TEXT("Player.%d"),Index);Slot.CharacterId=FString::Printf(TEXT("Character.%d"),Index);Slot.TeamId=Index<Spec.TeamSize?TEXT("team-a"):TEXT("team-b");Slot.HeroDefinitionId=TEXT("Test.LockedHero");Slot.SlotIndex=Index;Assignment.Roster.Add(Slot);}
            FString Error;if(!TestTrue(TEXT("五模式真实Assignment"),Mode->ApplyAssignmentWithModeSpec(Assignment,Spec,Error))){AddError(Error);return false;}
            for(const auto& Slot:Assignment.Roster)
            {
                auto* Controller=Owned.World->SpawnActor<AGamePlatformArenaPlayerController>();auto* State=Owned.World->SpawnActor<AGamePlatformArenaPlayerState>();
                if(!Controller||!State){AddError(TEXT("测试连接实体创建失败"));return false;}
                Controller->PlayerState=State;State->SetOwner(Controller);Owned.Lifecycle.Controllers.Add(Slot.PlayerId,Controller);
                FGamePlatformArenaTransferTicketClaims Claims;Claims.PlayerId=Slot.PlayerId;Claims.CharacterId=Slot.CharacterId;Claims.MatchId=Assignment.MatchId;Claims.DestinationServerId=Assignment.GameServerId;Claims.bConsumed=true;Claims.ExpiresAtUtc=FDateTime::UtcNow()+FTimespan::FromMinutes(1);
                if(!TestTrue(TEXT("真实Roster准入"),Mode->AdmitPlayer(State,Claims,Error))){AddError(Error);return false;}
            }
            TestEqual(TEXT("后端锁定英雄后直接进入十秒倒计时"),Mode->GetMatchPhase(),EGamePlatformArenaMatchPhase::Countdown);
            TestEqual(TEXT("批准倒计时10秒"),Mode->CountdownSeconds,10.f);
            Mode->StartMatchFromCountdown();TestEqual(TEXT("全部真实Pawn完成后开赛"),Mode->GetMatchPhase(),EGamePlatformArenaMatchPhase::InProgress);
            const int32 OriginalSpawns=Owned.Lifecycle.Spawns;
            if(Spec.TeamSize>=2&&bDoubleWipe)
            {
                const FString Player=Assignment.Roster[0].PlayerId;Mode->MarkPlayerDisconnected(Player);
                const int32 BeforeTimeout=Owned.Lifecycle.InactiveCalls;Mode->HandleReconnectTimeout(Player);
                TestEqual(TEXT("超时淘汰显式终结原实体资格"),Owned.Lifecycle.InactiveCalls,BeforeTimeout+1);
                Mode->ResolveEliminationAtEndFrame();TestEqual(TEXT("超时淘汰本人不代表整队失败"),Mode->GetMatchPhase(),EGamePlatformArenaMatchPhase::InProgress);
            }
            // 多人队伍的个人弃权只淘汰本人，帧末仍有双方存活时继续比赛。
            if(Spec.TeamSize>=2&&!bDoubleWipe)
            {
                auto* State=Owned.Lifecycle.Controllers.FindRef(Assignment.Roster[0].PlayerId)->GetPlayerState<AGamePlatformArenaPlayerState>();
                TestTrue(TEXT("个人弃权受理"),Mode->RequestForfeit(State,Error));
                Mode->ResolveEliminationAtEndFrame();
                TestTrue(TEXT("个人弃权标记本人淘汰"),Mode->IsPlayerEliminated(State->PlayerIdPublic));
                TestEqual(TEXT("个人不能代表整队投降"),Mode->GetMatchPhase(),EGamePlatformArenaMatchPhase::InProgress);
            }
            for(int32 Index=0;Index<(bDoubleWipe?Spec.TotalPlayers:Spec.TeamSize);++Index)
            {
                FGamePlatformArenaTrustedEvent Death;Death.EventId=FString::Printf(TEXT("death-%d"),Index);Death.PlayerId=Assignment.Roster[Index].PlayerId;Death.EventType=EGamePlatformArenaTrustedEventType::Death;
                if(!TestTrue(TEXT("环境死亡也淘汰"),Mode->HandleTrustedEvent(Death,Error))){AddError(Error);return false;}
                TestTrue(TEXT("同一事件重放幂等"),Mode->HandleTrustedEvent(Death,Error));
                if(Index==0&&Spec.TeamSize>=2&&!bDoubleWipe)
                {
                    const FString DeadPlayer=Death.PlayerId;Mode->MarkPlayerDisconnected(DeadPlayer);
                    auto* DeadController=Owned.World->SpawnActor<AGamePlatformArenaPlayerController>();auto* DeadState=Owned.World->SpawnActor<AGamePlatformArenaPlayerState>();
                    DeadController->PlayerState=DeadState;DeadState->SetOwner(DeadController);Owned.Lifecycle.Controllers.Add(DeadPlayer,DeadController);
                    TestTrue(TEXT("淘汰玩家可以恢复连接但不能复活"),Mode->TryReconnectPlayer(DeadState,DeadPlayer,Error));
                    TestTrue(TEXT("新PlayerState保留淘汰事实"),DeadState->bEliminated);TestEqual(TEXT("淘汰重连不调用实体恢复"),Owned.Lifecycle.Restores,0);
                    const FString LivingPlayer=Assignment.Roster.Last().PlayerId;auto* Original=Owned.Lifecycle.Pawns.FindRef(LivingPlayer);
                    Mode->MarkPlayerDisconnected(LivingPlayer);Owned.Lifecycle.Controllers.FindRef(LivingPlayer)->UnPossess();
                    auto* Controller=Owned.World->SpawnActor<AGamePlatformArenaPlayerController>();auto* State=Owned.World->SpawnActor<AGamePlatformArenaPlayerState>();Controller->PlayerState=State;State->SetOwner(Controller);Owned.Lifecycle.Controllers.Add(LivingPlayer,Controller);
                    TestTrue(TEXT("活玩家重连复用原实体端口"),Mode->TryReconnectPlayer(State,LivingPlayer,Error));TestTrue(TEXT("恢复同一Pawn"),Controller->GetPawn()==Original);TestEqual(TEXT("活重连不重新出生"),Owned.Lifecycle.Spawns,OriginalSpawns);
                }

            }
            TestEqual(TEXT("当前帧未提前抢先判胜"),Mode->GetMatchPhase(),EGamePlatformArenaMatchPhase::InProgress);
            TestEqual(TEXT("死亡不调用复活"),Owned.Lifecycle.Respawns,0);TestEqual(TEXT("死亡不创建新Pawn"),Owned.Lifecycle.Spawns,OriginalSpawns);
            // 调用实际绑定的帧末处理函数，避免在测试中广播全局引擎OnEndFrame影响其他世界。
            Mode->ResolveEliminationAtEndFrame();
            TestEqual(TEXT("完整终局进入结果待提交"),Mode->GetMatchPhase(),EGamePlatformArenaMatchPhase::ResultPending);
            TestEqual(TEXT("双方同帧全灭平局，否则另一队获胜"),Mode->GetPendingResult().WinningTeamId,bDoubleWipe?NAME_None:FName(TEXT("team-b")));
            TestEqual(TEXT("淘汰结束原因"),Mode->GetPendingResult().EndReason,EGamePlatformArenaMatchEndReason::Elimination);
        }
    }
    return true;
}
#endif
