#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Containers/Ticker.h"

// 删除精确解绑/分发中存活检查会让被撤销订阅、旧世界继续收到事件。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldReadinessNotificationTest,"GamePlatform.World.Lifecycle.ReadinessNotifications",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldReadinessNotificationTest::RunTest(const FString&)
{
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
 UWorld* Other=UWorld::CreateWorld(EWorldType::Game,false);
 auto* S=IGamePlatformWorldService::Get(*W);
 auto* Owner=W->SpawnActor<AActor>();
 int32 Calls=0;FGamePlatformResult Result;
 auto H=S->SubscribeReadiness(Owner,[&](const auto&){++Calls;},Result);
 TestTrue(TEXT("真实世界弱Owner可登记"),Result.IsSuccess()&&H.IsValid());
 TestEqual(TEXT("订阅不内联调用；调用者先自行读取完整快照"),Calls,0);
 auto Initial=S->GetReadiness();
 TestFalse(TEXT("未初始化区域定义不等于已加载"),Initial.bRegionDefinitionsLoaded);
 TestFalse(TEXT("跨World撤销被拒绝"),IGamePlatformWorldService::Get(*Other)->UnsubscribeReadiness(H));
 TestTrue(TEXT("本完整句柄撤销"),S->UnsubscribeReadiness(H));
 TestTrue(TEXT("本完整句柄重复撤销幂等"),S->UnsubscribeReadiness(H));
 W->bIsTearingDown=true;S->GetReadiness();
 FTSTicker::GetCoreTicker().Tick(.11f);
 TestEqual(TEXT("已撤销不接收退出通知"),Calls,0);
 W->DestroyWorld(false);Other->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldReadinessReentryTest,"GamePlatform.World.Lifecycle.ReadinessReentrantUnsubscribe",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldReadinessReentryTest::RunTest(const FString&)
{
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);auto* S=IGamePlatformWorldService::Get(*W);
 auto* Owner=W->SpawnActor<AActor>();FGamePlatformResult R;int32 Calls=0;
 FGamePlatformWorldRegistration H;
 H=S->SubscribeReadiness(Owner,[&](const auto& V){++Calls;TestTrue(TEXT("事件中可精确解绑"),S->UnsubscribeReadiness(H));TestTrue(TEXT("退出快照真实失效"),V.Context.ReadinessState==EGamePlatformWorldReadiness::Invalidated);},R);
 W->bIsTearingDown=true;S->GetReadiness();
 TestEqual(TEXT("退出发布一次"),Calls,1);
 S->GetReadiness();TestEqual(TEXT("重复读取不重复发事件"),Calls,1);
 W->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldReadinessOwnerTest,"GamePlatform.World.Lifecycle.ReadinessWeakOwnerAndWorldExit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldReadinessOwnerTest::RunTest(const FString&)
{
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);auto* S=IGamePlatformWorldService::Get(*W);auto* Owner=W->SpawnActor<AActor>();
 int32 DeadCalls=0,ExitCalls=0;FGamePlatformResult R;
 auto Dead=S->SubscribeReadiness(Owner,[&](const auto&){++DeadCalls;},R);
 auto Live=S->SubscribeReadiness(W,[&](const auto& V){if(V.Context.ReadinessState==EGamePlatformWorldReadiness::Invalidated)++ExitCalls;},R);
 TestTrue(TEXT("真实Owner销毁"),Owner->Destroy());
 W->GetSubsystem<UGamePlatformWorldSubsystem>()->OnWorldEndPlay(*W);
 TestEqual(TEXT("弱Owner销毁不再回调"),DeadCalls,0);TestEqual(TEXT("World退出真实终态一次"),ExitCalls,1);
 W->GetSubsystem<UGamePlatformWorldSubsystem>()->OnWorldEndPlay(*W);TestEqual(TEXT("重复退出幂等"),ExitCalls,1);
 TestTrue(TEXT("已退出完整订阅仍可幂等解绑"),S->UnsubscribeReadiness(Live));
 W->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldReadinessNestedExitTest,"GamePlatform.World.Lifecycle.ReadinessNestedExitTerminal",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldReadinessNestedExitTest::RunTest(const FString&)
{
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);auto* S=IGamePlatformWorldService::Get(*W);
 int32 Ordinary=0,Terminal[2]={0,0};FGamePlatformResult R;
 FGamePlatformWorldRegistration H[2];
 for(int32 I=0;I<2;++I)H[I]=S->SubscribeReadiness(W,[&,I](const auto& V){
  if(V.Context.ReadinessState==EGamePlatformWorldReadiness::Invalidated){++Terminal[I];return;}
  ++Ordinary;W->GetSubsystem<UGamePlatformWorldSubsystem>()->OnWorldEndPlay(*W);
 },R);
 FTSTicker::GetCoreTicker().Tick(.11f);
 TestEqual(TEXT("同步退出后旧普通快照不再分发"),Ordinary,1);
 TestEqual(TEXT("第一个仍有效订阅收到恰一次终态"),Terminal[0],1);
 TestEqual(TEXT("第二个仍有效订阅收到恰一次终态"),Terminal[1],1);
 W->GetSubsystem<UGamePlatformWorldSubsystem>()->OnWorldEndPlay(*W);
 FTSTicker::GetCoreTicker().Tick(.11f);
 TestEqual(TEXT("重复退出不再发布终态A"),Terminal[0],1);TestEqual(TEXT("重复退出不再发布终态B"),Terminal[1],1);
 W->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldReadinessNestedUnsubscribeTest,"GamePlatform.World.Lifecycle.ReadinessNestedExitUnsubscribe",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldReadinessNestedUnsubscribeTest::RunTest(const FString&)
{
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);auto* S=IGamePlatformWorldService::Get(*W);
 int32 Ordinary=0,Terminal[2]={0,0},Removed=INDEX_NONE;FGamePlatformResult R;FGamePlatformWorldRegistration H[2];
 for(int32 I=0;I<2;++I)H[I]=S->SubscribeReadiness(W,[&,I](const auto& V){
  if(V.Context.ReadinessState==EGamePlatformWorldReadiness::Invalidated){++Terminal[I];return;}
  ++Ordinary;W->GetSubsystem<UGamePlatformWorldSubsystem>()->OnWorldEndPlay(*W);
  Removed=1-I;TestTrue(TEXT("退出已排队后回调内仍能精确解绑"),S->UnsubscribeReadiness(H[Removed]));
 },R);
 FTSTicker::GetCoreTicker().Tick(.11f);
 TestEqual(TEXT("旧普通快照仅发一次"),Ordinary,1);
 if(TestTrue(TEXT("实际普通回调已覆盖"),Removed!=INDEX_NONE)){
  TestEqual(TEXT("已解绑者不接收排队终态"),Terminal[Removed],0);
  TestEqual(TEXT("仍有效者恰一次终态"),Terminal[1-Removed],1);
 }
 W->GetSubsystem<UGamePlatformWorldSubsystem>()->OnWorldEndPlay(*W);W->DestroyWorld(false);return true;
}
#endif
