#if WITH_DEV_AUTOMATION_TESTS
// 入口只发布活动意图；真实距离/视线与跨网由平台交互回归及联机检查，不以本测试代替。
#include "Misc/AutomationTest.h"
#include "Definitions/DivineBeastsArenaEntranceDefinition.h"
#include "Gameplay/DivineBeastsArenaEntrance.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Types/GamePlatformInteractionSession.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaEntranceContractTest,
    "DivineBeasts.Worlds.ArenaEntrance.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaEntranceContractTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Entrance=World->SpawnActor<ADivineBeastsArenaEntrance>();
    TestTrue(TEXT("未配置入口失败关闭"),Entrance->GetArenaModeId().IsNone());
    auto* Definition=NewObject<UDivineBeastsArenaEntranceDefinition>(Entrance);
    FGamePlatformId::TryParse(TEXT("divinebeasts.activityentrance.test@1"),Definition->LogicalId);
    Definition->ArenaModeId=TEXT("Arena.Mode.Unknown");
    Entrance->EntranceDefinition=Definition;
    Entrance->OnConstruction(Entrance->GetActorTransform());
    auto* Target=Entrance->FindComponentByClass<UGamePlatformInteractableComponent>();
    TestFalse(TEXT("未知模式不能交互"),Target->IsInteractionEnabled());
    Definition->ArenaModeId=TEXT("Arena.Mode.Team3v3");
    Entrance->OnConstruction(Entrance->GetActorTransform());
    TestEqual(TEXT("入口读取真实定义模式"),Entrance->GetArenaModeId(),Definition->ArenaModeId);
    TestTrue(TEXT("合法模式启用入口"),Target->IsInteractionEnabled());
    const auto* Option=Target->FindOption(TEXT("Arena.Entry.Open"));
    if (!TestNotNull(TEXT("只使用已约定活动选项"),Option)) return false;
    TestEqual(TEXT("多人共享入口"),Option->ConcurrencyPolicy,EGamePlatformInteractionConcurrencyPolicy::Shared);
    TestEqual(TEXT("不消耗入口或触发直接旅行"),Option->CommitKind,EGamePlatformInteractionCommitKind::Custom);
    TestEqual(TEXT("沿用250厘米服务器交互范围"),Option->MaxDistance,250.f);
    TestTrue(TEXT("继续要求服务器视线"),Option->bRequireLineOfSight);
    FGamePlatformInteractionSession Session;
    Session.RequestId=FGuid::NewGuid(); Session.SessionId=FGuid::NewGuid();
    Session.OptionId=Option->OptionId; Session.TargetActor=Entrance;
    TestTrue(TEXT("权威提交只确认活动意图"),Entrance->CommitInteraction(Session,*Option));
    TestFalse(TEXT("入口不会被消费"),Target->IsConsumed());
    FGamePlatformInteractionOption Wrong=*Option; Wrong.OptionId=TEXT("Unknown");
    TestFalse(TEXT("拒绝其它选项"),Entrance->CommitInteraction(Session,Wrong));
    return true;
}
#endif
