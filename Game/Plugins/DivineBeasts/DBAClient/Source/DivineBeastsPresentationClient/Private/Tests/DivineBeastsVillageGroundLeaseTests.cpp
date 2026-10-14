// 六纹理世界租约的真实异步集成：使用平台Data签发，不伪造Scope/Proof，不连接业务后端。
// 夹具世界不创建WeatherClient；验证静态草地仍可申请，旧代拒绝、真实释放及错误类型回滚。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/DivineBeastsVillageSnowWorldSubsystem.h"
#include "Settings/DivineBeastsVillageSnowSettings.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

class FDivineBeastsVillageGroundLeaseCommand final : public IAutomationLatentCommand
{
public:
    explicit FDivineBeastsVillageGroundLeaseCommand(FAutomationTestBase* InTest)
        : Test(InTest), Started(FPlatformTime::Seconds()) {}
    ~FDivineBeastsVillageGroundLeaseCommand() override
    {
        Alive.Reset(); // 销毁命令前撤销回调所有权，平台下一帧通知不得访问旧this。
        if (Service.IsValid()) { Service->bClosing = true; Service->ReleaseResources(); }
        if (Instance.IsValid())
        {
            UWorld* World = Instance->GetWorld();
            if (World) World->DestroyWorld(false);
            Instance->Shutdown();
            if (World) GEngine->DestroyWorldContext(World);
        }
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 30.)
        { Test->AddError(TEXT("六纹理真实异步租约超时")); return true; }
        if (Phase == 0)
        {
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
            UWorld* World = Instance->GetWorld();
            if (!Test->TestNotNull(TEXT("真实游戏实例世界"), World)) return true;
            Service.Reset(NewObject<UDivineBeastsVillageSnowWorldSubsystem>(World));
            Service->bVillageWorld = true;
            // 无WeatherClient绑定仍走同一生产资源入口，而不是天气Tick或测试LoadObject。
            Test->TestFalse(TEXT("夹具不绑定天气服务"), Service->WeatherClient.IsValid());
            Service->TryRequestSnowTextures();
            Lease = Service->TextureLease;
            if (!Test->TestTrue(TEXT("六纹理平台真实签发"), Lease.IsValid() && Lease.ResourcePaths.Num() == 6)) return true;
            Service->HandleTexturesReady(Service->RequestGeneration - 1, Lease, FGamePlatformResult::Success());
            Test->TestFalse(TEXT("资源未完成/旧代不能就绪"), Service->bTexturesReady);
            Phase = 1; return false;
        }
        IGamePlatformDataService* Data = IGamePlatformDataService::Get(*Instance);
        if (!Data) { Test->AddError(TEXT("真实Data消失")); return true; }
        if (Phase == 1)
        {
            const auto State = Data->GetLeaseState(Lease);
            if (State == EGamePlatformDataRequestState::Loading) return false;
            if (!Test->TestTrue(TEXT("真实六纹理异步成功"), State == EGamePlatformDataRequestState::Succeeded)) return true;
            Test->TestTrue(TEXT("真实成功回调启用就绪"), Service->bTexturesReady);
            Service->ReleaseResources();
            Test->TestTrue(TEXT("生产清理真实释放平台租约"), Data->GetLeaseState(Lease) == EGamePlatformDataRequestState::Released);
            Service->HandleTexturesReady(Service->RequestGeneration - 1, Lease, FGamePlatformResult::Success());
            Test->TestFalse(TEXT("释放后的迟到成功不能复活"), Service->bTexturesReady);
            Service->TryRequestSnowTextures();
            Cancelled = Service->TextureLease;
            Service->bClosing = true;
            Service->ReleaseResources();
            Test->TestTrue(TEXT("世界关闭在完成前取消新租约"), Data->GetLeaseState(Cancelled) == EGamePlatformDataRequestState::Released);
            Service->bClosing = false;
            // 用真实Material替代一张Texture，Data能加载，但项目类型合同必须拒绝并释放。
            auto Paths = Lease.ResourcePaths;
            Paths[0] = FSoftObjectPath(TEXT("/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge.M_DBA_Village_LandscapeBridge"));
            const int64 Generation = ++Service->RequestGeneration;
            const TWeakPtr<int32> Guard(Alive);
            const TWeakObjectPtr<UDivineBeastsVillageSnowWorldSubsystem> WeakService(Service.Get());
            FGamePlatformResult Accepted;
            Service->TextureLease = Data->AcquireResources(Paths, EGamePlatformDataLifetime::World, Service.Get(),
                [this, Guard, WeakService, Generation](const auto& Complete, const auto& Result)
                {
                    if (!Guard.IsValid()) return;
                    if (auto* S = WeakService.Get()) S->HandleTexturesReady(Generation, Complete, Result);
                    bWrongTypeCompleted = true;
                }, Accepted);
            WrongType = Service->TextureLease;
            Test->TestTrue(TEXT("错误类型由真实Data加载而非模拟"), Accepted.IsSuccess() && WrongType.IsValid());
            Phase = 2; return false;
        }
        if (!bWrongTypeCompleted) return false;
        Test->TestFalse(TEXT("真实材质资源不得当Texture成功"), Service->bTexturesReady);
        Test->TestFalse(TEXT("失败清空项目租约"), Service->TextureLease.IsValid());
        Test->TestTrue(TEXT("错误类型回滚释放平台资源"), Data->GetLeaseState(WrongType) == EGamePlatformDataRequestState::Released);
        return true;
    }
private:
    FAutomationTestBase* Test;
    double Started;
    int32 Phase = 0;
    bool bWrongTypeCompleted = false;
    TSharedPtr<int32> Alive = MakeShared<int32>(0);
    TStrongObjectPtr<UGameInstance> Instance;
    TStrongObjectPtr<UDivineBeastsVillageSnowWorldSubsystem> Service;
    FGamePlatformDataLease Lease, Cancelled, WrongType;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageGroundRealLeaseTest,
    "DivineBeasts.Presentation.VillageGround.RealAsyncLeaseCancellationAndTypeFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsVillageGroundRealLeaseTest::RunTest(const FString&)
{
    // 空夹具没有Landscape，真实成功会记录“未能绑定MID”；此处明确只验Data，不把错误记为通过。
    // 生产绑定失败日志属预期一次；完整64MID绑定另由实际地图PIE测试证明。
    AddExpectedError(TEXT("VILLAGE_SNOW_MID_VERIFY_FAILED"), EAutomationExpectedErrorFlags::Contains, 1);
    ADD_LATENT_AUTOMATION_COMMAND(FDivineBeastsVillageGroundLeaseCommand(this));
    return true;
}
#endif
