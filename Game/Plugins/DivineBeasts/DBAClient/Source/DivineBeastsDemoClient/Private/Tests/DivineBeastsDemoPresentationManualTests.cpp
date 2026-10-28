// 未在作者阶段执行；根应用后由真实UE目标编译/自动化运行。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Scenarios/DivineBeastsDemoPresentationScenarios.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAPresentationManualContractTest,"DivineBeasts.Demo.Presentation.Contract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBAPresentationManualContractTest::RunTest(const FString&)
{
    using namespace DivineBeasts::Demo;
    for(FName F:PresentationManualFeatures())
    {
        FDivineBeastsDemoScenarioDefinition D;D.FeatureId=F;D.AdapterId=FName(*(TEXT("PlatformPresentation.")+F.ToString()));
        D.ScenarioId=FName(*(F.ToString()+TEXT(".Manual")));D.Operations=PresentationManualOperations(F);
        TArray<FName> Names;TArray<EDivineBeastsDemoParameterType> Types;GetPresentationManualParameterSignature(F,Names,Types);
        for(int32 I=0;I<Names.Num();++I){FDivineBeastsDemoParameterDefinition P;P.ParameterId=Names[I];P.DefaultValue.Type=Types[I];D.Parameters.Add(P);}
        TestTrue(TEXT("稳定来源与真实操作合同"),ValidatePresentationManualContract(D));
        D.Operations.Add(TEXT("RunConsoleCommand"));TestFalse(TEXT("未知文本不能进入执行白名单"),ValidatePresentationManualContract(D));
        D.Operations=PresentationManualOperations(F);D.Operations.Last()=D.Operations[0];
        TestFalse(TEXT("重复操作不能冒充完整白名单"),ValidatePresentationManualContract(D));
        D.Operations=PresentationManualOperations(F);D.AdapterId=TEXT("Other.Adapter");TestFalse(TEXT("跨功能适配身份拒绝"),ValidatePresentationManualContract(D));
    }
    TestTrue(TEXT("Editor风通过自有世界入口"),PresentationManualOperations(TEXT("GamePlatformSurface.editor_wind")).Contains(TEXT("CreateEditorWorld")));
    TestFalse(TEXT("有限期痕迹真实提交入口"),PresentationManualOperations(TEXT("GamePlatformSurface.marks")).IsEmpty());
    TestFalse(TEXT("不提供影响其他请求的RefreshCatalog清理"),PresentationManualOperations(TEXT("GamePlatformSurface.marks")).Contains(TEXT("RefreshCatalog")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAPresentationManualFullHandleTest,"DivineBeasts.Demo.Presentation.FullHandle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBAPresentationManualFullHandleTest::RunTest(const FString&)
{
    TStrongObjectPtr<UWorld> W1(NewObject<UWorld>()),W2(NewObject<UWorld>());
    FGamePlatformSFXHandle A;A.Id=FGuid::NewGuid();A.Generation=2;A.World=W1.Get();auto B=A;
    TestTrue(TEXT("完整相同句柄"),DivineBeasts::Demo::PresentationManualSameSFXHandle(A,B));
    ++B.Generation;TestFalse(TEXT("同Request的新代句柄不消费旧完成"),DivineBeasts::Demo::PresentationManualSameSFXHandle(A,B));
    B=A;B.Id=FGuid::NewGuid();TestFalse(TEXT("相邻实例终态不互相消费"),DivineBeasts::Demo::PresentationManualSameSFXHandle(A,B));
    B=A;B.World=W2.Get();TestFalse(TEXT("相同GUID和代次跨世界仍拒绝"),DivineBeasts::Demo::PresentationManualSameSFXHandle(A,B));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAPresentationManualParameterTest,"DivineBeasts.Demo.Presentation.ParameterBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBAPresentationManualParameterTest::RunTest(const FString&)
{
    FDivineBeastsDemoScenarioDefinition D;FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Gain");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;P.Minimum=0;P.Maximum=1;D.Parameters.Add(P);
    auto V=P.DefaultValue;V.NumberValue=.5;TestTrue(TEXT("有限线性增益接受"),DivineBeasts::Demo::ValidatePresentationManualValues(D,{V}).IsSuccess());
    V.NumberValue=std::numeric_limits<double>::quiet_NaN();TestFalse(TEXT("NaN不进入真实服务"),DivineBeasts::Demo::ValidatePresentationManualValues(D,{V}).IsSuccess());
    V.NumberValue=2;TestFalse(TEXT("超范围增益拒绝"),DivineBeasts::Demo::ValidatePresentationManualValues(D,{V}).IsSuccess());
    V.Type=EDivineBeastsDemoParameterType::Identifier;TestFalse(TEXT("文本不隐式转换数值"),DivineBeasts::Demo::ValidatePresentationManualValues(D,{V}).IsSuccess());
    FGamePlatformUIThemeSnapshot S;S.ScopeId=FGuid::NewGuid();TestFalse(TEXT("空主题初态禁止正向修改"),DivineBeasts::Demo::CanBeginPresentationThemeFixture(S));
    S.ThemeId=FPrimaryAssetId(TEXT("GamePlatformDefinition"),TEXT("dba.ui.theme.default@1"));S.bRequestPending=true;TestFalse(TEXT("外部在途主题禁止接管"),DivineBeasts::Demo::CanBeginPresentationThemeFixture(S));
    S.bRequestPending=false;TestTrue(TEXT("有完整非空基线才可准备"),DivineBeasts::Demo::CanBeginPresentationThemeFixture(S));
    return true;
}
#endif
