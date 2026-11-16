// 真实平台注册表及临时World回归；无生产对象、无网络、无GUI/人工通过声明。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Scenarios/DivineBeastsDemoDiagnosticsDebugScenario.h"
#include "Registry/GamePlatformDebugRegistry.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
FDivineBeastsDemoScenarioContext MakeDebugContext(UWorld* World, bool bCommands = false)
{
    FDivineBeastsDemoScenarioContext Context;
    Context.World = World; Context.Token.RunId = FGuid::NewGuid(); Context.Token.WorldId = FGuid::NewGuid(); Context.Token.Generation = 1;
    Context.Scenario.AdapterId = TEXT("PlatformDebug");
    Context.Scenario.FeatureId = bCommands ? TEXT("GamePlatformDebug.command_catalog") : TEXT("GamePlatformDebug.snapshots");
    const TArray<FName> Ids = bCommands ? TArray<FName>{TEXT("MutatingDescriptor")} : TArray<FName>{TEXT("Counter"),TEXT("FieldCount"),TEXT("ValueLength"),TEXT("AllowExpensive"),TEXT("ExpensiveProvider")};
    for (int32 Index = 0; Index < Ids.Num(); ++Index)
    {
        auto& Parameter = Context.Scenario.Parameters.AddDefaulted_GetRef(); Parameter.ParameterId = Ids[Index];
        Parameter.DefaultValue.Type = !bCommands && Index < 3 ? EDivineBeastsDemoParameterType::Integer : EDivineBeastsDemoParameterType::Boolean;
    }
    return Context;
}
TArray<FDivineBeastsDemoParameterValue> DebugValues()
{
    TArray<FDivineBeastsDemoParameterValue> Values; Values.SetNum(5);
    for (int32 Index = 0; Index < 3; ++Index) Values[Index].Type = EDivineBeastsDemoParameterType::Integer;
    Values[3].Type=EDivineBeastsDemoParameterType::Boolean;Values[4].Type=EDivineBeastsDemoParameterType::Boolean;
    Values[0].IntegerValue = 709; Values[1].IntegerValue = 3; Values[2].IntegerValue = 32;
    return Values;
}
FGamePlatformResult DebugOperation(IDivineBeastsDemoScenarioAdapter& Adapter, FName Operation, const TArray<FDivineBeastsDemoParameterValue>& Values)
{
    FGamePlatformResult Result; int32 Calls = 0;
    Adapter.Execute(Operation, Values, [&Result,&Calls](const auto&,const auto& Delivered) { Result=Delivered; ++Calls; });
    if (Calls != 1) return FGamePlatformResult::Failure(TEXT("Test.CompletionCount"),TEXT("同步诊断操作须恰好完成一次。"));
    return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDebugManualOwnershipTest,
    "DivineBeasts.PluginDemos.DebugManualOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDebugManualOwnershipTest::RunTest(const FString&)
{
    auto& Registry = FGamePlatformDebugRegistry::Get(); const auto BeforeProviders=Registry.GetProviderIds(); const auto BeforeCommands=Registry.GetCommandDescriptors();
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false));
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto Adapter=DivineBeasts::Demo::CreateDiagnosticsDebugScenario(); auto Context=MakeDebugContext(World.Get());
    FGamePlatformResult Prepared; Adapter->Prepare(Context,[&Prepared](const auto&,const auto& Result){Prepared=Result;});
    if(!TestTrue(TEXT("真实世界样板准备"),Prepared.IsSuccess()))return false;
    const auto Values=DebugValues();
    TestTrue(TEXT("真实注册提供者"),DebugOperation(*Adapter,TEXT("RegisterProvider"),Values).IsSuccess());
    TestFalse(TEXT("重复注册拒绝并保留原归属"),DebugOperation(*Adapter,TEXT("RegisterProvider"),Values).IsSuccess());
    TestTrue(TEXT("重复请求之后原提供者仍能采集"),DebugOperation(*Adapter,TEXT("Execute"),Values).IsSuccess());
    TestTrue(TEXT("主动撤销"),DebugOperation(*Adapter,TEXT("UnregisterProvider"),Values).IsSuccess());
    FGamePlatformResult Stopped;Adapter->Stop([&Stopped](const auto&,const auto& Result){Stopped=Result;});
    TestTrue(TEXT("主动撤销之后Stop幂等清理"),Stopped.IsSuccess());
    TestTrue(TEXT("提供者集合精确恢复原样"),Registry.GetProviderIds()==BeforeProviders);
    auto Commands=DivineBeasts::Demo::CreateDiagnosticsDebugScenario();auto CommandContext=MakeDebugContext(World.Get(),true);
    Commands->Prepare(CommandContext,[&Prepared](const auto&,const auto& Result){Prepared=Result;});
    TestTrue(TEXT("命令元数据案例准备"),Prepared.IsSuccess());
    TArray<FDivineBeastsDemoParameterValue> CommandValues;CommandValues.AddDefaulted();
    CommandValues[0].Type=EDivineBeastsDemoParameterType::Boolean;
    TestTrue(TEXT("注册元数据"),DebugOperation(*Commands,TEXT("RegisterCommand"),CommandValues).IsSuccess());
    TestFalse(TEXT("重复元数据不得丢失归属"),DebugOperation(*Commands,TEXT("RegisterCommand"),CommandValues).IsSuccess());
    TestTrue(TEXT("元数据列表可观察"),DebugOperation(*Commands,TEXT("ListCommands"),CommandValues).IsSuccess());
    Commands->Stop([&Stopped](const auto&,const auto& Result){Stopped=Result;});
    TestTrue(TEXT("命令Stop成功"),Stopped.IsSuccess());
    TestEqual(TEXT("只撤本次描述，既有目录数量保持"),Registry.GetCommandDescriptors().Num(),BeforeCommands.Num());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDebugManualPrivacyTest,
    "DivineBeasts.PluginDemos.DebugManualPrivacy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDebugManualPrivacyTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false));ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto Adapter=DivineBeasts::Demo::CreateDiagnosticsDebugScenario();auto Context=MakeDebugContext(World.Get());FString Observation;
    Context.PublishObservation=[&Observation](const auto&,const FString& Text){Observation=Text;};
    FGamePlatformResult Prepared;Adapter->Prepare(Context,[&Prepared](const auto&,const auto& Result){Prepared=Result;});
    if(!TestTrue(TEXT("准备"),Prepared.IsSuccess()))return false;
    auto Values=DebugValues();Values[1].IntegerValue=256;Values[2].IntegerValue=1024;
    TestTrue(TEXT("注册"),DebugOperation(*Adapter,TEXT("RegisterProvider"),Values).IsSuccess());
    TestTrue(TEXT("真实平台脱敏和限长采集"),DebugOperation(*Adapter,TEXT("Execute"),Values).IsSuccess());
    TestFalse(TEXT("显式/字段名敏感样本不出现在UI投影"),Observation.Contains(TEXT("private-fixture-marker")));
    TestTrue(TEXT("观察桥仍有界"),!Observation.IsEmpty()&&Observation.Len()<=4096);
    TestTrue(TEXT("实际平台字段/字符串/负载限制触发截断"),Observation.Contains(TEXT("截断=是")));
    Values[4].bBooleanValue=true;Values[3].bBooleanValue=false;
    TestFalse(TEXT("昂贵采集缺许可被原生机制拒绝"),DebugOperation(*Adapter,TEXT("Execute"),Values).IsSuccess());
    Values[3].bBooleanValue=true;
    TestTrue(TEXT("显式许可后允许按需采集"),DebugOperation(*Adapter,TEXT("Execute"),Values).IsSuccess());
    TestTrue(TEXT("暂停原生提供者"),DebugOperation(*Adapter,TEXT("Pause"),Values).IsSuccess());
    TestFalse(TEXT("暂停后采集拒绝"),DebugOperation(*Adapter,TEXT("Execute"),Values).IsSuccess());
    TestTrue(TEXT("恢复原生提供者"),DebugOperation(*Adapter,TEXT("Resume"),Values).IsSuccess());
    TestTrue(TEXT("使真实弱目标失效"),DebugOperation(*Adapter,TEXT("InvalidateTarget"),Values).IsSuccess());
    TestFalse(TEXT("目标失效后不自动选其他Actor"),DebugOperation(*Adapter,TEXT("Execute"),Values).IsSuccess());
    FGamePlatformResult Stopped;Adapter->Stop([&Stopped](const auto&,const auto& Result){Stopped=Result;});
    TestTrue(TEXT("清理"),Stopped.IsSuccess());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDebugManualSpawnStopTest,
    "DivineBeasts.PluginDemos.DebugManualSpawnStop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDebugManualSpawnStopTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false));ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto Adapter=DivineBeasts::Demo::CreateDiagnosticsDebugScenario();auto Context=MakeDebugContext(World.Get());
    TWeakObjectPtr<AActor> Spawned;int32 StopCalls=0;FGamePlatformResult Stopped,Prepared;
    const auto Hook=World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([&](AActor* Actor)
    {
        Spawned=Actor;
        Adapter->Stop([&](const auto&,const auto& Result){Stopped=Result;++StopCalls;});
        TestEqual(TEXT("原生Spawn尚未返回时不得提前确认清理"),StopCalls,0);
    }));
    Adapter->Prepare(Context,[&Prepared](const auto&,const auto& Result){Prepared=Result;});World->RemoveOnActorSpawnedHandler(Hook);
    TestEqual(TEXT("停止只完成一次"),StopCalls,1);TestTrue(TEXT("迟到目标已清理后才成功"),Stopped.IsSuccess());
    TestFalse(TEXT("准备不能在Stop后恢复成功"),Prepared.IsSuccess());TestFalse(TEXT("不存在Stop后复活目标"),Spawned.IsValid());return true;
}
#endif
