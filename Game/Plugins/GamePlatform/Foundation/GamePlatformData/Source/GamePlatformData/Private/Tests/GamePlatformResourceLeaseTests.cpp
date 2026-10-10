// 平台双端数据真实资源回归：两个独立实例共享同一路径，提前取消、失败和释放均不撤销其他所有者。
#include "Interfaces/IGamePlatformDataService.h"
#include "Loading/GamePlatformAssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FResourceLeaseCommand final : public IAutomationLatentCommand
{
public:
    FResourceLeaseCommand(FAutomationTestBase* InTest, FPrimaryAssetId InId)
        : Test(InTest), Id(InId), Start(FPlatformTime::Seconds()) {}
    ~FResourceLeaseCommand() override
    {
        Alive.Reset();
        // 先结束租约，再关闭真实实例集合；不直接全局卸载夹具路径。
        for (auto* Instance : {A.Get(), B.Get()})
        {
            if (!Instance) continue;
            if (auto* Service = IGamePlatformDataService::Get(*Instance))
                for (const auto& Lease : {LeaseA, LeaseB, Cancelled, Missing})
                    if (Lease.IsValid() && Service->GetDiagnostics().ScopeId == Lease.ScopeId) Service->ReleaseResources(Lease);
            UWorld* World = Instance->GetWorld();
            if (World) World->DestroyWorld(false);
            Instance->Shutdown();
            if (World) GEngine->DestroyWorldContext(World);
        }
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Start > 30.0) { Test->AddError(TEXT("真实普通资源租约超时")); return true; }
        if (Phase == 0)
        {
            auto* Manager = Cast<UGamePlatformAssetManager>(UAssetManager::GetIfInitialized());
            if (!Manager || Manager->GetPrimaryAssetPath(Id).IsNull())
            { Test->AddError(TEXT("需正式唯一AssetManager与实际已注册夹具资产")); return true; }
            Path = Manager->GetPrimaryAssetPath(Id);
            A.Reset(NewObject<UGameInstance>(GEngine)); B.Reset(NewObject<UGameInstance>(GEngine));
            A->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); B->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
            auto* SA = IGamePlatformDataService::Get(*A); auto* SB = IGamePlatformDataService::Get(*B);
            if (!SA || !SB) { Test->AddError(TEXT("真实实例数据服务未创建")); return true; }
            FGamePlatformResult Accepted;
            const TWeakPtr<int32> Guard(Alive);
            LeaseA = SA->AcquireResources({Path, Path}, EGamePlatformDataLifetime::Instance, A.Get(),
                [this, Guard](auto, auto Result) { if (Guard.IsValid()) { ++CountA; ResultA = Result; } }, Accepted);
            Test->TestTrue(TEXT("A同步接纳且去重"), Accepted.IsSuccess() && LeaseA.ResourcePaths.Num() == 1 && CountA == 0);
            LeaseB = SB->AcquireResources({Path}, EGamePlatformDataLifetime::Instance, B.Get(),
                [this, Guard](auto, auto Result) { if (Guard.IsValid()) { ++CountB; ResultB = Result; } }, Accepted);
            Cancelled = SA->AcquireResources({Path}, EGamePlatformDataLifetime::Instance, A.Get(),
                [this, Guard](auto, auto Result) { if (Guard.IsValid()) { ++CountCancelled; Test->TestTrue(TEXT("提前取消明确终态"), Result.Status == EGamePlatformResultStatus::Cancelled); } }, Accepted);
            SA->ReleaseResources(Cancelled);
            Missing = SA->AcquireResources({FSoftObjectPath(TEXT("/Game/DoesNotExist_LeaseTest.Asset"))}, EGamePlatformDataLifetime::Instance, A.Get(),
                [this, Guard](auto, auto Result) { if (Guard.IsValid()) { ++CountMissing; Test->TestFalse(TEXT("缺资源明确失败"), Result.IsSuccess()); } }, Accepted);
            Test->TestFalse(TEXT("跨实例拒绝释放"), SB->ReleaseResources(LeaseA).IsSuccess());
            Phase = 1; return false;
        }
        if (!CountA || !CountB || !CountCancelled || !CountMissing) return false;
        Test->TestTrue(TEXT("同一路径两实例成功"), ResultA.IsSuccess() && ResultB.IsSuccess());
        auto* SA = IGamePlatformDataService::Get(*A); auto* SB = IGamePlatformDataService::Get(*B);
        auto Forged = LeaseA; Forged.ResourcePaths.Add(FSoftObjectPath(TEXT("/Game/Forged.Asset")));
        Test->TestFalse(TEXT("篡改路径不能释放"), SA->ReleaseResources(Forged).IsSuccess());
        SA->ReleaseResources(LeaseA);
        Test->TestTrue(TEXT("重复原件释放幂等"), SA->ReleaseResources(LeaseA).IsSuccess());
        Test->TestTrue(TEXT("A释放后B仍持有"), SB->GetLeaseState(LeaseB) == EGamePlatformDataRequestState::Succeeded);
        Test->TestNotNull(TEXT("B资源仍驻留"), Path.ResolveObject());
        Test->TestEqual(TEXT("无释放历史"), SA->GetDiagnostics().ReleasedLeaseRecords, 0);
        Test->TestEqual(TEXT("A仅一次终态"), CountA, 1); Test->TestEqual(TEXT("B仅一次终态"), CountB, 1);
        return true;
    }
private:
    FAutomationTestBase* Test;
    FPrimaryAssetId Id;
    FSoftObjectPath Path;
    double Start;
    int32 Phase = 0, CountA = 0, CountB = 0, CountCancelled = 0, CountMissing = 0;
    TStrongObjectPtr<UGameInstance> A, B;
    TSharedPtr<int32> Alive = MakeShared<int32>(0);
    FGamePlatformDataLease LeaseA, LeaseB, Cancelled, Missing;
    FGamePlatformResult ResultA, ResultB;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformResourceLeaseIntegrationTest, "GamePlatform.Data.Runtime.ResourceLeases",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformResourceLeaseIntegrationTest::RunTest(const FString& Parameters)
{
    FString Definition;
    if (!FParse::Value(FCommandLine::Get(), TEXT("GamePlatformDataTestDefinition="), Definition))
    { AddError(TEXT("必须指定已生成真实夹具的GamePlatformDataTestDefinition；缺资产不能假通过")); return false; }
    ADD_LATENT_AUTOMATION_COMMAND(FResourceLeaseCommand(this, FPrimaryAssetId(Definition)));
    return true;
}
#endif
