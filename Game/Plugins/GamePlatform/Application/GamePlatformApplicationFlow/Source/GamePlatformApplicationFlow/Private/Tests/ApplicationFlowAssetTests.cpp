#if WITH_DEV_AUTOMATION_TESTS
#include "API/GamePlatformApplicationFlowSubsystem.h"
#include "Definitions/GamePlatformFlowDefinition.h"
#include "Engine/GameInstance.h"
#include "Interfaces/GamePlatformCallbackFlowNode.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformFlowAssetValidationTest, "GamePlatform.ApplicationFlow.Asset.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformFlowAssetValidationTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGamePlatformFlowDefinition> Asset(NewObject<UGamePlatformFlowDefinition>());
    FGamePlatformId::TryParse(TEXT("platform.tests.flow@1"), Asset->LogicalId);
    Asset->EntryNodeId = TEXT("Entry");
    FGamePlatformFlowNodeDefinition Node;
    Node.NodeId = TEXT("entry"); Node.ExecutorId = TEXT("Wait"); Node.NextNodeId = TEXT("Entry");
    Asset->Nodes.Add(Node);
    TestFalse(TEXT("资产默认同样拒绝循环"), Asset->ValidateDefinition().IsSuccess());
    Asset->bAllowCycles = true;
    TestTrue(TEXT("显式循环资产可以通过结构校验"), Asset->ValidateDefinition().IsSuccess());
    Asset->MaxImmediateCycleTransitions = 0;
    TestFalse(TEXT("零预算拒绝"), Asset->ValidateDefinition().IsSuccess());
    Asset->MaxImmediateCycleTransitions = 64;
    Asset->Nodes[0].ExecutorId = NAME_None;
    TestFalse(TEXT("空执行器拒绝"), Asset->ValidateDefinition().IsSuccess());
    Asset->Nodes[0].ExecutorId = TEXT("Wait");
    Asset->Nodes[0].InputDefinitionId = FPrimaryAssetId(TEXT("OtherType"), TEXT("invalid"));
    TestFalse(TEXT("非法输入定义类型拒绝"), Asset->ValidateDefinition().IsSuccess());
    Asset->Nodes[0].InputDefinitionId = FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), TEXT("platform.tests.input@1"));
    TestTrue(TEXT("合法中立输入定义"), Asset->ValidateDefinition().IsSuccess());
    Asset->Nodes[0].Routes.Add(TEXT("missing"), TEXT("Unknown"));
    TestFalse(TEXT("循环模式不放过悬空分支"), Asset->ValidateDefinition().IsSuccess());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformFlowFactoryRegistrationTest, "GamePlatform.ApplicationFlow.Asset.FactoryScope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformFlowFactoryRegistrationTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGameInstance> OtherInstance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformApplicationFlowSubsystem> Service(NewObject<UGamePlatformApplicationFlowSubsystem>(Instance.Get()));
    TStrongObjectPtr<UGamePlatformApplicationFlowSubsystem> Other(NewObject<UGamePlatformApplicationFlowSubsystem>(OtherInstance.Get()));
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    Service->Initialize(Collection); Other->Initialize(Collection);
    int32 FactoryCalls = 0;
    FGamePlatformFlowNodeFactory Factory = [&](UGameInstance& Owner) { ++FactoryCalls; return NewObject<UGamePlatformCallbackFlowNode>(&Owner); };
    FGamePlatformResult Result;
    const auto Handle = Service->RegisterNodeFactory(TEXT("Wait"), Factory, Result);
    TestTrue(TEXT("注册成功"), Handle.IsValid() && Result.IsSuccess());
    TestEqual(TEXT("注册不执行工厂"), FactoryCalls, 0);
    TestFalse(TEXT("重复键拒绝"), Service->RegisterNodeFactory(TEXT("wait"), Factory, Result).IsValid());
    TestFalse(TEXT("跨实例不能撤销"), Other->UnregisterNodeFactory(Handle, Result));
    TestTrue(TEXT("正确注册可撤销"), Service->UnregisterNodeFactory(Handle, Result));
    const auto NewHandle = Service->RegisterNodeFactory(TEXT("Wait"), Factory, Result);
    TestFalse(TEXT("旧句柄不能撤销新注册"), Service->UnregisterNodeFactory(Handle, Result));
    TestTrue(TEXT("新注册仍存活"), Service->UnregisterNodeFactory(NewHandle, Result));
    FGamePlatformDataLease Lease;
    Lease.ScopeId = FGuid::NewGuid(); Lease.LeaseId = FGuid::NewGuid(); Lease.Generation = 1;
    Lease.DefinitionId = FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), TEXT("platform.tests.flow@1"));
    const auto OriginalLeaseId = Lease.LeaseId;
    TestFalse(TEXT("伪造就绪值不能启动"), Service->StartFlow(Lease, nullptr, Result).IsValid());
    TestFalse(TEXT("拒绝提供真实失败结果"), Result.IsSuccess());
    TestEqual(TEXT("同步拒绝不转移租约"), Lease.LeaseId, OriginalLeaseId);
    TestEqual(TEXT("拒绝前不创建节点"), FactoryCalls, 0);
    Service->Deinitialize(); Other->Deinitialize();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformFlowEventTokenTest, "GamePlatform.ApplicationFlow.Asset.EventToken",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformFlowEventTokenTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformApplicationFlowSubsystem> Service(NewObject<UGamePlatformApplicationFlowSubsystem>(Instance.Get()));
    FSubsystemCollection<UGameInstanceSubsystem> Collection; Service->Initialize(Collection);
    FGamePlatformFlowContext Context;
    auto* Node = NewObject<UGamePlatformCallbackFlowNode>(Instance.Get());
    Node->Bind([&](const auto& InContext, auto) { Context = InContext; }, [](auto) {});
    FGamePlatformFlowDefinition Definition; Definition.EntryNodeId = TEXT("Wait");
    FGamePlatformFlowStep Step; Step.NodeId = TEXT("Wait"); Step.Node = Node; Definition.Steps.Add(Step);
    FString Error; Service->Configure(Definition, Error); Service->Start(nullptr, Error);
    FTSTicker::GetCoreTicker().Tick(0.01f);
    TestFalse(TEXT("旧配置上下文输入为空"), Context.InputDefinitionId.IsValid());
    TestTrue(TEXT("节点代次非零"), Context.NodeGeneration != 0);
    FGamePlatformFlowNodeToken Token{Context.Handle, Context.NodeId, Context.NodeGeneration};
    FGamePlatformResult Result;
    auto Wrong = Token; Wrong.Handle.ScopeId = FGuid::NewGuid();
    TestFalse(TEXT("跨实例事件拒绝"), Service->SubmitEvent(Wrong, FGamePlatformFlowNodeResult::Success(), Result));
    TestFalse(TEXT("跨实例精确取消拒绝"), Service->CancelFlow(Wrong, Result));
    TestTrue(TEXT("匹配令牌投递成功"), Service->SubmitEvent(Token, FGamePlatformFlowNodeResult::Success(), Result));
    TestFalse(TEXT("重复事件拒绝"), Service->SubmitEvent(Token, FGamePlatformFlowNodeResult::Success(), Result));
    FTSTicker::GetCoreTicker().Tick(0.01f);
    TestTrue(TEXT("邮箱消费后成功"), Service->GetSnapshot().State == EGamePlatformFlowState::Succeeded);
    Service->Start(nullptr, Error); FTSTicker::GetCoreTicker().Tick(0.01f);
    TestFalse(TEXT("旧运行令牌不能取消新运行"), Service->CancelFlow(Token, Result));
    const FGamePlatformFlowNodeToken NewToken{Context.Handle, Context.NodeId, Context.NodeGeneration};
    TestTrue(TEXT("当前节点精确取消"), Service->CancelFlow(NewToken, Result));
    Service->Deinitialize(); return true;
}
#endif
