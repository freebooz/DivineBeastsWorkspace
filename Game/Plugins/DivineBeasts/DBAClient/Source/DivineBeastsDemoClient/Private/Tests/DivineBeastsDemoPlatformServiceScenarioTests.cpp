// T04原生回归源码；稳定窗口仅交源码，不宣称已运行RED/GREEN。
#if WITH_DEV_AUTOMATION_TESTS
#include "Scenarios/DivineBeastsDemoPlatformServiceScenarios.h"
#include "Misc/AutomationTest.h"
#include "Scenarios/DivineBeastsDemoPlatformManualPorts.h"
namespace DivineBeasts::Demo {bool VerifyManualSaveCleanupForTests();
#if WITH_EDITOR
bool VerifyManualServerOwnershipForTests();
#endif
}
#include "Engine/World.h"
namespace
{
FDivineBeastsDemoScenarioContext MakeManualContext(UWorld& World,FName Feature)
{
    FDivineBeastsDemoScenarioContext C;C.World=&World;C.Token.RunId=FGuid::NewGuid();C.Token.WorldId=FGuid::NewGuid();C.Token.Generation=1;
    C.Scenario.FeatureId=Feature;C.Scenario.ScenarioId=FName(*(Feature.ToString()+TEXT(".Manual")));
    C.Scenario.AdapterId=FName(*(FString(TEXT("PlatformServices."))+Feature.ToString()));
    TArray<FName> Names,Operations;TArray<EDivineBeastsDemoParameterType> Types;
    DivineBeasts::Demo::GetPlatformManualSignature(Feature,Names,Types,Operations);C.Scenario.Operations=Operations;
    for(int32 I=0;I<Names.Num();++I){auto& P=C.Scenario.Parameters.AddDefaulted_GetRef();P.ParameterId=Names[I];P.DefaultValue.Type=Types[I];}
    return C;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformManualVersionBoundary,"DivineBeasts.PluginDemos.PlatformServices.CoreVersionBoundary",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlatformManualVersionBoundary::RunTest(const FString&)
{
    TArray<FDivineBeastsDemoParameterValue> V;
    for(int32 I=0;I<9;++I){auto& P=V.AddDefaulted_GetRef();P.Type=EDivineBeastsDemoParameterType::Integer;}
    V[0].IntegerValue=2;V[3].IntegerValue=1;V[6].IntegerValue=3;
    TestTrue(TEXT("真实Core比较/范围运算"),DivineBeasts::Demo::EvaluateManualCoreVersion(V).IsSuccess());
    V[6].IntegerValue=0;
    TestFalse(TEXT("逆序范围拒绝且不夹紧"),DivineBeasts::Demo::EvaluateManualCoreVersion(V).IsSuccess());
    V[6].IntegerValue=3;V[0].IntegerValue=int64(MAX_int32)+1;
    TestFalse(TEXT("溢出不能截断为版本"),DivineBeasts::Demo::EvaluateManualCoreVersion(V).IsSuccess());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformManualContractRejects,"DivineBeasts.PluginDemos.PlatformServices.ClosedContract",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlatformManualContractRejects::RunTest(const FString&)
{
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
    if(!TestNotNull(TEXT("独立测试World"),W))return false;
    auto C=MakeManualContext(*W,TEXT("GamePlatformCore.03"));
    TestTrue(TEXT("真实签名接受"),DivineBeasts::Demo::ValidatePlatformManualContract(C.Scenario));
    C.Scenario.Operations.Add(TEXT("ExecuteArbitraryConsole"));
    TestFalse(TEXT("无反射/控制台后门操作"),DivineBeasts::Demo::ValidatePlatformManualContract(C.Scenario));
    C=MakeManualContext(*W,TEXT("GamePlatformCore.03"));
    Swap(C.Scenario.Parameters[0],C.Scenario.Parameters[1]);
    TestFalse(TEXT("位置互换不能静默误解释"),DivineBeasts::Demo::ValidatePlatformManualContract(C.Scenario));
    C=MakeManualContext(*W,TEXT("GamePlatformCore.03"));C.Scenario.Parameters[0].DefaultValue.Type=EDivineBeastsDemoParameterType::Identifier;
    TestFalse(TEXT("类型混淆拒绝"),DivineBeasts::Demo::ValidatePlatformManualContract(C.Scenario));
    TestFalse(TEXT("未知功能不造空成功适配"),DivineBeasts::Demo::CreatePlatformServiceScenario(*W,TEXT("Unknown.Plugin")).IsValid());
    W->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformManualSourceFrozen,"DivineBeasts.PluginDemos.PlatformServices.FrozenSource",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlatformManualSourceFrozen::RunTest(const FString&)
{
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
    if(!TestNotNull(TEXT("独立测试World"),W))return false;
    auto Adapter=DivineBeasts::Demo::CreatePlatformServiceScenario(*W,TEXT("GamePlatformCore.03"));
    if(!TestTrue(TEXT("真实适配创建"),Adapter.IsValid())){W->DestroyWorld(false);return false;}
    auto C=MakeManualContext(*W,TEXT("GamePlatformCore.03"));const auto Original=C.Token;
    bool Prepared=false;Adapter->Prepare(C,[&](const auto& Token,const auto& R){Prepared=R.IsSuccess();TestTrue(TEXT("准备返回原Token"),Token==Original);});
    TestTrue(TEXT("首次准备成功"),Prepared);
    auto Other=C;Other.Token.RunId=FGuid::NewGuid();Other.Token.WorldId=FGuid::NewGuid();Other.Token.Generation=2;
    Adapter->Prepare(Other,[&](const auto&,const auto& R){TestFalse(TEXT("同实例不能重绑定Token来源"),R.IsSuccess());});
    TArray<FDivineBeastsDemoParameterValue> Values;for(const auto& P:C.Scenario.Parameters)Values.Add(P.DefaultValue);
    Adapter->Execute(TEXT("CompareVersions"),Values,[&](const auto& Token,const auto& R)
        {TestTrue(TEXT("拒绝重绑定后仍返回固定原Token"),Token==Original);TestTrue(TEXT("仍执行原真实Core操作"),R.IsSuccess());});
    Adapter->Stop([&](const auto& Token,const auto& R){TestTrue(TEXT("停止返回原Token"),Token==Original);TestTrue(TEXT("值能力真实清理"),R.IsSuccess());});
    Adapter->Execute(TEXT("CompareVersions"),Values,[&](const auto& Token,const auto& R)
        {TestTrue(TEXT("停止后不会换来源"),Token==Original);TestFalse(TEXT("停止后拒绝新操作"),R.IsSuccess());});
    Adapter.Reset();W->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformManualTypedDelivery,"DivineBeasts.PluginDemos.PlatformServices.TypedDeliveryReentry",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlatformManualTypedDelivery::RunTest(const FString&)
{
    using namespace DivineBeasts::Demo;
    TManualPortPending<TFunction<void()>> Slot;const auto Scope=FGuid::NewGuid();uint64 Sequence=0;int32 Calls=0;
    TestTrue(TEXT("存储原请求"),Slot.Store(Scope,1,Sequence,TEXT("Reconcile"),TEXT("OwnOrder"),[&](){++Calls;Slot.Reset();}));
    TestFalse(TEXT("未交付请求不覆盖"),Slot.Store(Scope,1,Sequence,TEXT("Purchase"),TEXT("OtherOrder"),[](){}));
    FManualPortIdentity Identity;
    TestFalse(TEXT("同序号不同类型拒绝"),bool(Slot.Take(TEXT("Purchase"),1,Identity)));
    TestFalse(TEXT("旧序号拒绝"),bool(Slot.Take(TEXT("Reconcile"),2,Identity)));
    auto Callback=Slot.Take(TEXT("Reconcile"),1,Identity);
    TestTrue(TEXT("完成端先移出存储"),bool(Callback) && !bool(Slot));Callback();
    TestEqual(TEXT("同步重入清空存储仍安全执行一次"),Calls,1);
    TestTrue(TEXT("身份保留原Scope/类型/代次"),Identity.ScopeId==Scope && Identity.Generation==1 && Identity.Kind==TEXT("Reconcile"));
    TestTrue(TEXT("新代次可再次接收"),Slot.Store(Scope,2,Sequence,TEXT("Reconcile"),TEXT("OwnOrder"),[](){}));
    TestFalse(TEXT("旧序号不能消费新完成端"),bool(Slot.Take(TEXT("Reconcile"),1,Identity)));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformManualSaveOwnership,"DivineBeasts.PluginDemos.PlatformServices.SaveCleanupOwnership",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlatformManualSaveOwnership::RunTest(const FString&)
{return TestTrue(TEXT("失服务保留资源且新清理尝试不沿用旧失败"),DivineBeasts::Demo::VerifyManualSaveCleanupForTests());}
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformManualServerOwnership,"DivineBeasts.PluginDemos.PlatformServices.ServerProviderOwnership",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPlatformManualServerOwnership::RunTest(const FString&)
{return TestTrue(TEXT("双来源Provider隔离、错误类型不消费、撤销后拒绝及外部Operation拒绝"),DivineBeasts::Demo::VerifyManualServerOwnershipForTests());}
#endif

#endif

