#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformWorldSubsystem.h"
#include "Engine/World.h"
#include "Services/GamePlatformWorldServices.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldTypeTest,"GamePlatform.World.Lifecycle.WorldTypes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldTypeTest::RunTest(const FString&)
{
    const auto* CDO=GetDefault<UGamePlatformWorldSubsystem>();
    TestTrue(TEXT("Game"),CDO->DoesSupportWorldType(EWorldType::Game));
    TestTrue(TEXT("PIE"),CDO->DoesSupportWorldType(EWorldType::PIE));
    for(auto Type:{EWorldType::Editor,EWorldType::EditorPreview,EWorldType::GamePreview,EWorldType::Inactive,EWorldType::None})
        TestFalse(TEXT("非运行类型不创建"),CDO->DoesSupportWorldType(Type));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldLifecycleTest,"GamePlatform.World.Lifecycle.GenerationAndTeardown",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldLifecycleTest::RunTest(const FString&)
{
    UWorld* A=UWorld::CreateWorld(EWorldType::Game,false);
    UWorld* B=UWorld::CreateWorld(EWorldType::Game,false);
    auto* SA=IGamePlatformWorldService::Get(*A);auto* SB=IGamePlatformWorldService::Get(*B);
    if(TestNotNull(TEXT("A服务"),SA)&&TestNotNull(TEXT("B服务"),SB))
    {
        const auto GA=SA->GetReadiness().Context.ContextGeneration;
        const auto GB=SB->GetReadiness().Context.ContextGeneration;
        TestTrue(TEXT("独立代次"),GA.IsValid()&&GB.IsValid()&&GA!=GB);
        TestFalse(TEXT("无定义不Ready"),SA->GetReadiness().Context.ReadinessState==EGamePlatformWorldReadiness::Ready);
        TestFalse(TEXT("拒绝未实现Session"),SA->InitializeSessionWorld().IsSuccess());
        FGamePlatformId Out;FGamePlatformId::TryParse(TEXT("test.region@1"),Out);
        TestTrue(TEXT("没有Provider查询空区域"),SA->QueryRegion(FVector::ZeroVector,Out).IsSuccess()&&!Out.IsValid());
        A->bIsTearingDown=true;
        TestTrue(TEXT("公开读取立即失效，不等下一Tick"),SA->GetReadiness().Context.ReadinessState==EGamePlatformWorldReadiness::Invalidated);
        A->DestroyWorld(false);A=nullptr;
        TestTrue(TEXT("A退出不改变B"),SB->GetReadiness().Context.ContextGeneration==GB);
    }
    if(A)A->DestroyWorld(false);B->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldLoadingLifetimeTest,"GamePlatform.World.Loading.ReleasedTaskNotReady",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldLoadingLifetimeTest::RunTest(const FString&)
{
    auto Task=GamePlatformWorldServices::CreateReadinessTask();
    TestFalse(TEXT("未绑定世界不能借用默认Ready"),Task->IsReadyToUse());
    Task->Release();Task->Release();
    TestFalse(TEXT("释放幂等且不可使用"),Task->IsReadyToUse());
    TestTrue(TEXT("过期轮询失败"),Task->Poll().State==EGamePlatformLoadingTaskUpdate::Failed);return true;
}
#endif
