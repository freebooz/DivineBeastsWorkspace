#include "Interfaces/IGamePlatformLoadingService.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/PlatformTime.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
/** 仅测试工厂与资源生命周期，不扮演Session/网络/真实资产。 */
struct FLoadingTestSignals { bool bFinish = false; int32 Starts = 0; int32 Releases = 0; };
class FLoadingTestTask final : public IGamePlatformLoadingTask
{
public:
    explicit FLoadingTestTask(TSharedRef<FLoadingTestSignals> InSignals) : Signals(InSignals) {}
    FGamePlatformResult Start(UGameInstance&,const FGamePlatformLoadingTaskSpec&) override
    { ++Signals->Starts; return FGamePlatformResult::Success(); }
    FGamePlatformLoadingTaskUpdate Poll() override
    { return {Signals->bFinish ? EGamePlatformLoadingTaskUpdate::Succeeded : EGamePlatformLoadingTaskUpdate::Pending,1,NAME_None}; }
    void Release() override { ++Signals->Releases; }
private:
    TSharedRef<FLoadingTestSignals> Signals;
};
class FLoadingIntegrationCommand final : public IAutomationLatentCommand
{
public:
    FLoadingIntegrationCommand(FAutomationTestBase* InTest,bool bInUseRealData) : Test(InTest),bUseRealData(bInUseRealData) {}
    ~FLoadingIntegrationCommand() override { Cleanup(); }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 35) { Test->AddError(TEXT("Loading真实服务测试超时")); return true; }
        if (Phase == 0)
        {
            Instance.Reset(NewObject<UGameInstance>(GEngine)); Other.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); Other->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
            auto* Loading = IGamePlatformLoadingService::Get(*Instance);
            if (!Loading || !IGamePlatformLoadingService::Get(*Other)) { Test->AddError(TEXT("真实Loading实例子系统缺失")); return true; }
            FGamePlatformLoadingOperationSpec Spec; Spec.Purpose = TEXT("Automation"); FGamePlatformResult Result;
            if (bUseRealData)
            {
                for (const TCHAR* Id : {TEXT("foundation.probe@1"),TEXT("foundation.flow@1")})
                {
                    FGamePlatformLoadingTaskSpec Task; Task.TaskId = FName(Id); Task.TaskType = TEXT("Data");
                    Task.Data.DefinitionId = FPrimaryAssetId(TEXT("GamePlatformDefinition"),FName(Id)); Spec.Tasks.Add(Task);
                }
            }
            else
            {
                Factory = Loading->RegisterTaskFactory(TEXT("TestLatch"),[Shared = Signals]() { return MakeUnique<FLoadingTestTask>(Shared); },Result);
                Test->TestTrue(TEXT("工厂注册"),Result.IsSuccess());
                Loading->RegisterTaskFactory(TEXT("TestLatch"),[Shared = Signals]() { return MakeUnique<FLoadingTestTask>(Shared); },Result);
                Test->TestFalse(TEXT("重复工厂拒绝"),Result.IsSuccess());
                FGamePlatformLoadingTaskSpec Task; Task.TaskId = TEXT("Latch"); Task.TaskType = TEXT("TestLatch"); Spec.Tasks.Add(Task);
                // 仅测试配置GC所有权，不实例化这个临时类，也不将其冒充已加载定义。
                auto* ConfigurationClass = NewObject<UClass>(GetTransientPackage());
                ConfigurationClass->SetSuperStruct(UGamePlatformDefinitionBase::StaticClass());
                ConfiguredClass = ConfigurationClass;
                Spec.Tasks[0].Data.ExpectedClass = ConfigurationClass;
                Spec.Tasks[0].FallbackData.ExpectedClass = ConfigurationClass;
            }
            Handle = Loading->StartLoadingOperation(Spec,Instance.Get(),Result);
            Test->TestTrue(TEXT("真实服务接纳"),Result.IsSuccess());
            Loading->StartLoadingOperation(Spec,Instance.Get(),Result); Test->TestFalse(TEXT("不覆盖旧操作"),Result.IsSuccess());
            Test->TestFalse(TEXT("跨实例取消拒绝"),IGamePlatformLoadingService::Get(*Other)->CancelLoadingOperation(Handle).IsSuccess());
            Phase = 1; return false;
        }
        auto* Loading = IGamePlatformLoadingService::Get(*Instance);
        const auto Snapshot = Loading->GetLoadingSnapshot();
        if (Snapshot.State == EGamePlatformLoadingState::Failed || Snapshot.State == EGamePlatformLoadingState::TimedOut)
        { Test->AddError(TEXT("真实Loading任务失败；Data测试要求实际已生成两份定义，不能跳过")); return true; }
        if (!bUseRealData && Phase == 1)
        {
            if (Signals->Starts == 0 || Snapshot.OverallProgress01 < 1) { return false; }
            Test->TestFalse(TEXT("100%并非Ready"),Loading->IsReadyToPlay(Handle));
            CollectGarbage(RF_NoFlags);
            Test->TestTrue(TEXT("异步等待期间冻结配置类受GC保活"),ConfiguredClass.IsValid());
            Test->TestFalse(TEXT("活动工厂不可撤销"),Loading->UnregisterTaskFactory(Factory).IsSuccess());
            Signals->bFinish = true; Phase = 2; return false;
        }
        if (!Loading->IsReadyToPlay(Handle)) { return false; }
        if (bUseRealData)
        {
            auto* Data = IGamePlatformDataService::Get(*Instance);
            Test->TestEqual(TEXT("Ready仍持有两份真实租约"),Data->GetDiagnostics().ActiveLeases,2);
            Loading->ReleaseLoadingOperation(Handle);
            Test->TestEqual(TEXT("释放仅撤销自己的两份租约"),Data->GetDiagnostics().ActiveLeases,0);
        }
        else
        {
            Test->TestEqual(TEXT("成功不立即释放"),Signals->Releases,0);
            Loading->ReleaseLoadingOperation(Handle); Loading->ReleaseLoadingOperation(Handle);
            Test->TestEqual(TEXT("幂等释放一次"),Signals->Releases,1);
            Test->TestTrue(TEXT("释放后可撤销工厂"),Loading->UnregisterTaskFactory(Factory).IsSuccess());
            FGamePlatformLoadingOperationSpec Spec; Spec.Purpose = TEXT("AfterUnregister");
            FGamePlatformLoadingTaskSpec Task; Task.TaskId = TEXT("Latch"); Task.TaskType = TEXT("TestLatch"); Spec.Tasks.Add(Task);
            FGamePlatformResult Result; Loading->StartLoadingOperation(Spec,Instance.Get(),Result);
            Test->TestFalse(TEXT("撤销工厂不能用于新操作"),Result.IsSuccess());
            Spec.Tasks[0].TaskType = TEXT("SessionReady"); Loading->StartLoadingOperation(Spec,Instance.Get(),Result);
            Test->TestEqual(TEXT("不伪造会话准入"),Result.Code,FName(TEXT("SessionPrerequisiteMissing")));
            Spec.Tasks[0].TaskType = TEXT("WorldPresence"); Spec.TargetWorldPackage = TEXT("/Game/NoSuchWorld");
            Handle = Loading->StartLoadingOperation(Spec,Instance.Get(),Result);
            Test->TestTrue(TEXT("目标世界待满足"),Result.IsSuccess());
            if (Other->GetWorld()) { Test->TestFalse(TEXT("另一个实例世界不可报告"),Loading->ReportWorldOperable(Handle,*Other->GetWorld()).IsSuccess()); }
            Loading->CancelLoadingOperation(Handle); Test->TestFalse(TEXT("取消世界等待不Ready"),Loading->IsReadyToPlay(Handle));
        }
        return true;
    }
private:
    void Cleanup()
    {
        for (auto* Owner : {Instance.Get(),Other.Get()})
        {
            if (!Owner) { continue; }
            UWorld* World = Owner->GetWorld(); if (World) { World->DestroyWorld(false); }
            Owner->Shutdown(); if (World) { GEngine->DestroyWorldContext(World); }
        }
        Instance.Reset(); Other.Reset();
    }
    FAutomationTestBase* Test;
    bool bUseRealData;
    double Started = FPlatformTime::Seconds();
    int32 Phase = 0;
    TStrongObjectPtr<UGameInstance> Instance,Other;
    TSharedRef<FLoadingTestSignals> Signals = MakeShared<FLoadingTestSignals>();
    FGamePlatformLoadingHandle Handle;
    FGamePlatformLoadingRegistration Factory;
    TWeakObjectPtr<UClass> ConfiguredClass;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadingServiceLifecycleTest,"GamePlatform.Loading.Service.Lifecycle",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoadingServiceLifecycleTest::RunTest(const FString&)
{ ADD_LATENT_AUTOMATION_COMMAND(FLoadingIntegrationCommand(this,false)); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadingRealDataTest,"GamePlatform.Loading.Data.RealLeases",EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLoadingRealDataTest::RunTest(const FString&)
{ ADD_LATENT_AUTOMATION_COMMAND(FLoadingIntegrationCommand(this,true)); return true; }
#endif
