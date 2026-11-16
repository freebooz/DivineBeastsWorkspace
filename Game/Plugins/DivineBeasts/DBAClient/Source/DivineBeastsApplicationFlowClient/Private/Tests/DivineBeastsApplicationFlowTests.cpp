#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/GameInstance.h"
#include "Definitions/GamePlatformFlowDefinition.h"
#include "Context/DivineBeastsApplicationFlowContext.h"
#include "Flow/DivineBeastsFlowNodes.h"
#include "Flow/DivineBeastsFlowTypes.h"
#include "Extensions/DivineBeastsApplicationFlowExtension.h"
#include "DivineBeastsApplicationFlowSubsystem.h"
#include "API/GamePlatformApplicationFlowSubsystem.h"
#include "Containers/Ticker.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Interfaces/GamePlatformCallbackFlowNode.h"
#include "Misc/ScopeExit.h"
#include "Subsystems/SubsystemCollection.h"

/**
 * 验证项目层只声明稳定NodeId/ExecutorId，不再依赖旧RegisterNode/TransitionTo状态机。
 * 这里只检查数据驱动身份边界；真实运行由GamePlatformApplicationFlow资产集成测试覆盖。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsFlowIdentityContractTest,
    "DivineBeasts.ApplicationFlow.IdentityContract",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsFlowIdentityContractTest::RunTest(const FString&)
{
    const TArray<FName> NodeIds =
    {
        FDivineBeastsFlowNodes::Boot(),
        FDivineBeastsFlowNodes::Initialize(),
        FDivineBeastsFlowNodes::Authentication(),
        FDivineBeastsFlowNodes::LoadProfile(),
        FDivineBeastsFlowNodes::LoadRoster(),
        FDivineBeastsFlowNodes::CharacterEntry(),
        FDivineBeastsFlowNodes::CreateCharacter(),
        FDivineBeastsFlowNodes::ValidateSelection(),
        FDivineBeastsFlowNodes::ResolveExperience(),
        FDivineBeastsFlowNodes::RequestWorld(),
        FDivineBeastsFlowNodes::TransferWorld(),
        FDivineBeastsFlowNodes::WorldReady(),
        FDivineBeastsFlowNodes::InWorld(),
        FDivineBeastsFlowNodes::Recovering()
    };
    const TArray<FName> ExecutorIds =
    {
        FDivineBeastsFlowExecutors::Boot(),
        FDivineBeastsFlowExecutors::Initialize(),
        FDivineBeastsFlowExecutors::Authentication(),
        FDivineBeastsFlowExecutors::LoadProfile(),
        FDivineBeastsFlowExecutors::LoadRoster(),
        FDivineBeastsFlowExecutors::CharacterEntry(),
        FDivineBeastsFlowExecutors::CreateCharacter(),
        FDivineBeastsFlowExecutors::ValidateSelection(),
        FDivineBeastsFlowExecutors::ResolveExperience(),
        FDivineBeastsFlowExecutors::RequestWorld(),
        FDivineBeastsFlowExecutors::TransferWorld(),
        FDivineBeastsFlowExecutors::WorldReady(),
        FDivineBeastsFlowExecutors::InWorld(),
        FDivineBeastsFlowExecutors::Recovering()
    };

    TSet<FName> UniqueNodes(NodeIds);
    TSet<FName> UniqueExecutors(ExecutorIds);
    TestEqual(TEXT("项目流程节点身份必须唯一"), UniqueNodes.Num(), NodeIds.Num());
    TestEqual(TEXT("项目流程执行器身份必须唯一"), UniqueExecutors.Num(), ExecutorIds.Num());

    FGamePlatformFlowNodeDefinition Sample;
    Sample.NodeId = FDivineBeastsFlowNodes::CharacterEntry();
    Sample.ExecutorId = FDivineBeastsFlowExecutors::CharacterEntry();
    Sample.Routes.Add(
        FDivineBeastsFlowOutcomes::CreateCharacter(),
        FDivineBeastsFlowNodes::CreateCharacter());
    Sample.Routes.Add(
        FDivineBeastsFlowOutcomes::SelectCharacter(),
        FDivineBeastsFlowNodes::ValidateSelection());
    TestEqual(TEXT("角色创建路由进入创建节点"),
        Sample.Routes[FDivineBeastsFlowOutcomes::CreateCharacter()],
        FDivineBeastsFlowNodes::CreateCharacter());
    TestEqual(TEXT("角色选择路由进入验证节点"),
        Sample.Routes[FDivineBeastsFlowOutcomes::SelectCharacter()],
        FDivineBeastsFlowNodes::ValidateSelection());
    return true;
}

/**
 * 验证项目流程上下文只承载跨节点业务值，敏感连接材料必须一次性消费。
 * 测试不依赖地图或网络，因此可快速发现上下文生命周期回归。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsFlowContextLifetimeTest,
    "DivineBeasts.ApplicationFlow.ContextLifetime",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsFlowContextLifetimeTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    UDivineBeastsApplicationFlowContext* Context =
        NewObject<UDivineBeastsApplicationFlowContext>(Instance.Get());
    TestNotNull(TEXT("项目流程上下文必须可由GameInstance拥有"), Context);
    if (!Context)
    {
        return false;
    }

    Context->ResetForNewRun(true);
    TestTrue(TEXT("自动登录策略保存在项目上下文"), Context->ShouldTryAutoLogin());

    FDivineBeastsWorldAssignmentSummary Summary;
    Summary.AssignmentId = TEXT("assignment-1");
    Summary.GameServerId = TEXT("server-1");
    Summary.ExperienceId = TEXT("Experience.OpenWorld.Hub");
    Summary.WorldId = TEXT("world.open@1");
    Context->SetAssignment(Summary, TEXT("127.0.0.1:7777"), TEXT("secret-ticket"));

    FString Endpoint;
    FString Ticket;
    TestTrue(TEXT("第一次允许取走连接材料"), Context->ConsumeConnectionMaterial(Endpoint, Ticket));
    TestFalse(TEXT("票据消费后不能再次复用"), Context->ConsumeConnectionMaterial(Endpoint, Ticket));
    // 人工登录是正常首屏策略；开启过自动恢复后，新运行也不能继承旧策略或连接材料。
    Context->ResetForNewRun(false);
    TestFalse(TEXT("人工登录运行不触发自动恢复"), Context->ShouldTryAutoLogin());
    TestFalse(TEXT("重启人工登录运行清除旧准入票据"), Context->ConsumeConnectionMaterial(Endpoint, Ticket));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsFlowTypeBoundaryTest,
    "DivineBeasts.ApplicationFlow.TypeBoundary",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsFlowTypeBoundaryTest::RunTest(const FString&)
{
    FDivineBeastsCharacterCreateDraft Draft;
    TestFalse(TEXT("空角色草稿必须非法"), Draft.IsLocallyValid());

    Draft.HeroDefinitionId = TEXT("Hero.Rat");
    Draft.CharacterName = TEXT("RatHero");
    TestTrue(TEXT("最小合法角色草稿通过本地结构校验"), Draft.IsLocallyValid());

    TestEqual(
        TEXT("持久角色选择动作必须显式存在"),
        StaticEnum<EDivineBeastsFlowAction>()->GetNameStringByValue(
            static_cast<int64>(EDivineBeastsFlowAction::SelectPersistentCharacter)),
        FString(TEXT("SelectPersistentCharacter")));
    TestEqual(
        TEXT("世界体验请求动作必须显式存在"),
        StaticEnum<EDivineBeastsFlowAction>()->GetNameStringByValue(
            static_cast<int64>(EDivineBeastsFlowAction::RequestExperience)),
        FString(TEXT("RequestExperience")));

    TestTrue(
        TEXT("认证错误模型包含错误凭据"),
        StaticEnum<EDivineBeastsFlowError>()->IsValidEnumValue(
            static_cast<int64>(EDivineBeastsFlowError::InvalidCredentials)));
    TestTrue(
        TEXT("流程错误模型包含执行失败"),
        StaticEnum<EDivineBeastsFlowError>()->IsValidEnumValue(
            static_cast<int64>(EDivineBeastsFlowError::FlowExecutionFailed)));
    return true;
}

namespace
{
    class FTestFlowExtension final : public IDivineBeastsApplicationFlowExtension
    {
    public:
        virtual void OnEnteredInWorld(
            UDivineBeastsApplicationFlowSubsystem&) override
        {
            ++Entered;
        }

        virtual void OnLeavingInWorld(
            UDivineBeastsApplicationFlowSubsystem& Flow) override
        {
            ++Left;
            if (OnLeaving)
            {
                OnLeaving(Flow);
            }
        }

        int32 Entered = 0;
        int32 Left = 0;
        TFunction<void(UDivineBeastsApplicationFlowSubsystem&)> OnLeaving;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsFlowExtensionRegistryTest,
    "DivineBeasts.ApplicationFlow.ExtensionRegistry",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsFlowExtensionRegistryTest::RunTest(const FString&)
{
    // UGameInstanceSubsystem声明Within=GameInstance；测试夹具必须使用合法Outer，
    // 否则UE5.8会在StaticAllocateObject阶段触发ensure，无法验证真正的扩展注册行为。
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    UDivineBeastsApplicationFlowSubsystem* Flow =
        NewObject<UDivineBeastsApplicationFlowSubsystem>(Instance.Get());
    TestNotNull(TEXT("项目流程子系统必须由GameInstance拥有并可构造"), Flow);
    if (!Flow)
    {
        return false;
    }

    const TSharedRef<FTestFlowExtension> Extension =
        MakeShared<FTestFlowExtension>();
    const FName ExtensionId(TEXT("Test.Arena.Extension"));

    TestTrue(TEXT("扩展首次注册成功"), Flow->RegisterExtension(ExtensionId, Extension));
    TestFalse(TEXT("重复扩展身份拒绝"), Flow->RegisterExtension(ExtensionId, Extension));
    TestTrue(TEXT("扩展注销成功"), Flow->UnregisterExtension(ExtensionId));
    TestFalse(TEXT("旧扩展身份重复注销安全失败"), Flow->UnregisterExtension(ExtensionId));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldTransferIntentTest,
    "DivineBeasts.ApplicationFlow.WorldTransfer.AcceptedIntentWins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsWorldTransferIntentTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformApplicationFlowSubsystem> Service(
        NewObject<UGamePlatformApplicationFlowSubsystem>(Instance.Get()));
    TStrongObjectPtr<UDivineBeastsApplicationFlowSubsystem> Flow(
        NewObject<UDivineBeastsApplicationFlowSubsystem>(Instance.Get()));
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    Service->Initialize(Collection);
    ON_SCOPE_EXIT { Service->Deinitialize(); };

    Flow->PlatformFlow = Service.Get();
    Flow->FlowContext = NewObject<UDivineBeastsApplicationFlowContext>(Instance.Get());
    FDivineBeastsCharacterSummary Character;
    Character.CharacterId = TEXT("character-intent-test");
    Flow->FlowContext->SetSelectedCharacter(Character);

    auto* Node = NewObject<UGamePlatformCallbackFlowNode>(Instance.Get());
    Node->Bind([](const auto&, auto) {}, [](auto) {});
    FGamePlatformFlowDefinition Definition;
    Definition.EntryNodeId = FDivineBeastsFlowNodes::InWorld();
    auto& Step = Definition.Steps.AddDefaulted_GetRef();
    Step.NodeId = Definition.EntryNodeId;
    Step.Node = Node;
    Step.Routes.Add(FDivineBeastsFlowOutcomes::RequestWorld(), NAME_None);
    FString Error;
    if (!TestTrue(TEXT("配置实际流程邮箱"), Service->Configure(Definition, Error)))
    {
        return false;
    }
    Flow->ActiveFlow = Service->Start(Flow->FlowContext.Get(), Error);
    FTSTicker::GetCoreTicker().Tick(0.01f);
    if (!TestTrue(TEXT("世界节点已开始"), Flow->IsCurrentNode(FDivineBeastsFlowNodes::InWorld())))
    {
        return false;
    }

    const auto Extension = MakeShared<FTestFlowExtension>();
    const FName First = FDivineBeastsProjectCatalog::GetVillageTutorialExperience();
    const FName Second = FDivineBeastsProjectCatalog::GetVillageTrainingExperience();
    bool bReentrantAccepted = false;
    Extension->OnLeaving = [&](UDivineBeastsApplicationFlowSubsystem& Active)
    {
        if (Extension->Left == 1)
        {
            bReentrantAccepted = Active.RequestWorldAssignment(Second, TEXT("reentrant"));
        }
    };
    Flow->RegisterExtension(TEXT("Test.WorldTransfer"), Extension);
    TestTrue(TEXT("首条转场被受理"), Flow->RequestWorldAssignment(First, TEXT("first")));
    TestFalse(TEXT("离场回调重入不得接管首条转场"), bReentrantAccepted);
    TestFalse(TEXT("同帧重复请求被拒绝"), Flow->RequestWorldAssignment(Second, TEXT("second")));
    TestEqual(TEXT("仅被受理的请求写入目标"), Flow->FlowContext->GetTargetExperience(), First);
    TestEqual(TEXT("被拒绝的请求不能改写区域"), Flow->FlowContext->GetPreferredRegion(), FString(TEXT("first")));
    TestEqual(TEXT("离场通知只发一次"), Extension->Left, 1);
    return true;
}

#endif
