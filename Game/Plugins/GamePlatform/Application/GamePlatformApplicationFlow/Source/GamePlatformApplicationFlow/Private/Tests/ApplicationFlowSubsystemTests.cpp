#if WITH_DEV_AUTOMATION_TESTS
#include "API/GamePlatformApplicationFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Interfaces/GamePlatformCallbackFlowNode.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformFlowScopeTest, "GamePlatform.ApplicationFlow.Adapter.ScopeAndCancellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformFlowScopeTest::RunTest(const FString& Parameters)
{
    // 手动装配隔离 GameInstance，不要求地图、账号、网络或现有业务插件。
    TStrongObjectPtr<UGameInstance> LeftInstance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGameInstance> RightInstance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformApplicationFlowSubsystem> Left(NewObject<UGamePlatformApplicationFlowSubsystem>(LeftInstance.Get()));
    TStrongObjectPtr<UGamePlatformApplicationFlowSubsystem> Right(NewObject<UGamePlatformApplicationFlowSubsystem>(RightInstance.Get()));
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    Left->Initialize(Collection);
    Right->Initialize(Collection);
    int32 Starts = 0;
    int32 Finishes = 0;
    int32 Terminals = 0;
    FGamePlatformFlowCompletion LateCompletion;
    auto* Node = NewObject<UGamePlatformCallbackFlowNode>(LeftInstance.Get());
    TestTrue(TEXT("组合根绑定执行与清理"), Node->Bind(
        [&](const auto&, auto Complete) { ++Starts; LateCompletion = MoveTemp(Complete); },
        [&](auto Reason) { ++Finishes; TestTrue(TEXT("清理原因为取消"), Reason == EGamePlatformFlowFinishReason::Cancelled); }));
    FGamePlatformFlowDefinition Definition;
    Definition.EntryNodeId = TEXT("Entry");
    FGamePlatformFlowStep Step;
    Step.NodeId = TEXT("entry"); // FName 大小写不敏感：适配核心后仍应成功。
    Step.Node = Node;
    Definition.Steps.Add(Step);
    FString Error;
    TestFalse(TEXT("跨 GameInstance 注入应拒绝"), Right->Configure(Definition, Error));
    TestTrue(TEXT("本 GameInstance 注入成功"), Left->Configure(Definition, Error));
    Left->OnFinished().AddLambda([&](const auto& Snapshot)
    {
        ++Terminals;
        TestTrue(TEXT("广播取消终态"), Snapshot.State == EGamePlatformFlowState::Cancelled);
        TestFalse(TEXT("终态广播中拒绝重入启动"), Left->Start(nullptr, Error).IsValid());
    });
    const auto Handle = Left->Start(nullptr, Error);
    TestTrue(TEXT("句柄有效"), Handle.IsValid());
    TestFalse(TEXT("跨作用域取消无效"), Right->Cancel(Handle));
    FTSTicker::GetCoreTicker().Tick(0.01f);
    TestEqual(TEXT("仅执行一次"), Starts, 1);
    TestTrue(TEXT("本作用域取消成功"), Left->Cancel(Handle));
    TestFalse(TEXT("重复取消无效"), Left->Cancel(Handle));
    if (LateCompletion) LateCompletion(FGamePlatformFlowNodeResult::Success());
    FTSTicker::GetCoreTicker().Tick(0.01f);
    TestEqual(TEXT("只清理一次"), Finishes, 1);
    TestEqual(TEXT("只广播一次"), Terminals, 1);
    Left->Deinitialize(); Right->Deinitialize();
    TestEqual(TEXT("关闭已终结流程不重复广播"), Terminals, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformFlowShutdownTest, "GamePlatform.ApplicationFlow.Adapter.Shutdown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformFlowShutdownTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformApplicationFlowSubsystem> Service(NewObject<UGamePlatformApplicationFlowSubsystem>(Instance.Get()));
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    Service->Initialize(Collection);
    int32 FinishCount = 0;
    int32 TerminalCount = 0;
    FGamePlatformFlowCompletion LateCompletion;
    auto* Node = NewObject<UGamePlatformCallbackFlowNode>(Instance.Get());
    Node->Bind([&](const auto&, auto Complete) { LateCompletion = MoveTemp(Complete); },
        [&](auto Reason) { ++FinishCount; TestTrue(TEXT("关闭清理原因"), Reason == EGamePlatformFlowFinishReason::Shutdown); });
    FGamePlatformFlowDefinition Definition;
    Definition.EntryNodeId = TEXT("Wait");
    FGamePlatformFlowStep Step; Step.NodeId = TEXT("Wait"); Step.Node = Node; Definition.Steps.Add(Step);
    FString Error; TestTrue(TEXT("安装"), Service->Configure(Definition, Error));
    Service->OnFinished().AddLambda([&](const auto& Snapshot)
    {
        ++TerminalCount;
        TestTrue(TEXT("退出广播Shutdown"), Snapshot.State == EGamePlatformFlowState::Shutdown);
    });
    TestTrue(TEXT("启动"), Service->Start(nullptr, Error).IsValid());
    FTSTicker::GetCoreTicker().Tick(0.01f);
    Service->Deinitialize();
    if (LateCompletion) LateCompletion(FGamePlatformFlowNodeResult::Success());
    TestEqual(TEXT("关闭清理一次"), FinishCount, 1);
    TestEqual(TEXT("关闭终态一次"), TerminalCount, 1);
    TestFalse(TEXT("关闭后不可启动"), Service->Start(nullptr, Error).IsValid());
    return true;
}
/** 回归：业务回调可能触发宿主退出；不能在执行器自己的调用栈内释放它。 */
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FGamePlatformFlowReentrantShutdownTest,
    "GamePlatform.ApplicationFlow.Adapter.ReentrantShutdown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FGamePlatformFlowReentrantShutdownTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    for (const TCHAR* Scenario : {TEXT("Execute"), TEXT("FinishSuccess"), TEXT("FinishCancel"), TEXT("FinishShutdown"), TEXT("TerminalEvent")})
    {
        Names.Add(Scenario);
        Commands.Add(Scenario);
    }
}

bool FGamePlatformFlowReentrantShutdownTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformApplicationFlowSubsystem> Service(NewObject<UGamePlatformApplicationFlowSubsystem>(Instance.Get()));
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    Service->Initialize(Collection);
    int32 FinishCount = 0;
    int32 TerminalCount = 0;
    FGamePlatformFlowCompletion LateCompletion;
    auto* Node = NewObject<UGamePlatformCallbackFlowNode>(Instance.Get());
    Node->Bind([&](const auto&, auto Complete)
    {
        LateCompletion = Complete;
        if (Parameters == TEXT("Execute")) Service->Deinitialize();
        if (Parameters == TEXT("FinishSuccess") || Parameters == TEXT("TerminalEvent"))
            Complete(FGamePlatformFlowNodeResult::Success());
    }, [&](auto Reason)
    {
        ++FinishCount;
        if (Parameters.StartsWith(TEXT("Finish"))) Service->Deinitialize();
        const auto ExpectedReason = Parameters == TEXT("FinishCancel") ? EGamePlatformFlowFinishReason::Cancelled :
            ((Parameters == TEXT("FinishSuccess") || Parameters == TEXT("TerminalEvent")) ?
                EGamePlatformFlowFinishReason::Succeeded : EGamePlatformFlowFinishReason::Shutdown);
        TestTrue(TEXT("保留退出原因，不二次清理"), Reason == ExpectedReason);
    });
    FGamePlatformFlowDefinition Definition;
    Definition.EntryNodeId = TEXT("Entry");
    FGamePlatformFlowStep Step; Step.NodeId = TEXT("Entry"); Step.Node = Node; Definition.Steps.Add(Step);
    FString Error;
    TestTrue(TEXT("配置回归场景"), Service->Configure(Definition, Error));
    Service->OnFinished().AddLambda([&](const auto& Snapshot)
    {
        ++TerminalCount;
        if (Parameters == TEXT("TerminalEvent")) Service->Deinitialize();
        const auto ExpectedState = Parameters == TEXT("FinishCancel") ? EGamePlatformFlowState::Cancelled :
            ((Parameters == TEXT("FinishSuccess") || Parameters == TEXT("TerminalEvent")) ?
                EGamePlatformFlowState::Succeeded : EGamePlatformFlowState::Shutdown);
        TestTrue(TEXT("终态仅广播一次并保留已完成的结果"), Snapshot.State == ExpectedState);
    });
    const auto Handle = Service->Start(nullptr, Error);
    TestTrue(TEXT("启动回归场景"), Handle.IsValid());
    FTSTicker::GetCoreTicker().Tick(0.01f);
    if (Parameters == TEXT("FinishCancel")) TestTrue(TEXT("取消触发清理内关闭"), Service->Cancel(Handle));
    if (Parameters == TEXT("FinishShutdown")) Service->Deinitialize();
    FTSTicker::GetCoreTicker().Tick(0.01f);
    TestEqual(TEXT("退出只清理一次"), FinishCount, 1);
    TestEqual(TEXT("退出只广播一次"), TerminalCount, 1);
    TestTrue(TEXT("分发栈展开后完成关闭"), Service->GetSnapshot().State == EGamePlatformFlowState::Shutdown);
    TestFalse(TEXT("关闭后不可重新启动"), Service->Start(nullptr, Error).IsValid());
    if (LateCompletion) LateCompletion(FGamePlatformFlowNodeResult::Success());
    Service->Deinitialize();
    TestEqual(TEXT("重复关闭幂等"), FinishCount, 1);
    return true;
}
#endif
