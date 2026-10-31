// 只验证项目协调器的注册失败、唯一身份和清理边界；不伪造Data成功、不代表Niagara/GPU或人工视觉通过。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/DivineBeastsArenaEntranceVFXWorldSubsystem.h"
#include "Gameplay/DivineBeastsArenaEntrance.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

struct FDivineBeastsArenaEntranceVFXTestAccess
{
    static void Observe(UDivineBeastsArenaEntranceVFXWorldSubsystem& Service,AActor* Actor){Service.ObserveActor(Actor);}
    static int32 Gates(const UDivineBeastsArenaEntranceVFXWorldSubsystem& Service){return Service.Bindings.Num();}
    static bool HasRequests(const UDivineBeastsArenaEntranceVFXWorldSubsystem& Service)
    {for(const auto& Pair:Service.Bindings)if(Pair.Value.Request.RequestId.IsValid())return true;return false;}
    static void Remove(UDivineBeastsArenaEntranceVFXWorldSubsystem& Service,AActor* Actor){Service.RemoveActor(Actor);}
    static void OwnRequest(UDivineBeastsArenaEntranceVFXWorldSubsystem& Service,AActor* Actor,UGamePlatformPresentationClientSubsystem& Adapter,const FGuid& Id)
    {
        auto& Binding=Service.Bindings.FindChecked(Actor);
        Binding.Adapter=&Adapter;Binding.Request.RequestId=Id;
        Binding.Request.SemanticTag=Service.GetVortexSemantic();Binding.Request.WorldGeneration=Adapter.GetWorldGeneration();
        Binding.Request.ProviderChannel=TEXT("VFX");Binding.Request.DefinitionId=TEXT("divinebeasts.vfx.village_arena_gate_vortex@1");
    }
    static bool LateActivationIgnored(UDivineBeastsArenaEntranceVFXWorldSubsystem& Service)
    {
        Service.OwnerToken=FGuid::NewGuid();Service.PackHandle.Id=FGuid::NewGuid();Service.PackHandle.ScopeId=FGuid::NewGuid();Service.PackHandle.Generation=1;
        const auto Handle=Service.PackHandle;
        Service.HandlePack(Handle,EDivineBeastsPresentationContentPackState::Active,FString(),FGuid::NewGuid());
        return !Service.bPackReady&&!HasRequests(Service);
    }
    static bool RejectUnavailableRegistration(UDivineBeastsArenaEntranceVFXWorldSubsystem& Service,
        UDivineBeastsPresentationClientSubsystem& Project,UGamePlatformPresentationClientSubsystem& Platform)
    {
        Service.Presentation=&Project;Service.PlatformPresentation=&Platform;Service.bStarted=true;Service.OwnerToken=FGuid::NewGuid();
        Service.EnsureContentPack();Service.EnsureContentPack();
        return !Service.PackHandle.IsValid()&&!Service.bPackReady&&!HasRequests(Service);
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaEntranceVFXLifecycleTest,
    "DivineBeasts.Arena.VFX.EntranceLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaEntranceVFXLifecycleTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    const TStrongObjectPtr<UDivineBeastsArenaEntranceVFXWorldSubsystem> Service(NewObject<UDivineBeastsArenaEntranceVFXWorldSubsystem>(World));
    ON_SCOPE_EXIT {Service->Deinitialize();World->DestroyWorld(false);};
    auto* Gate=World->SpawnActor<ADivineBeastsArenaEntrance>();
    Service->OnWorldBeginPlay(*World);
    FDivineBeastsArenaEntranceVFXTestAccess::Observe(*Service,Gate);
    FDivineBeastsArenaEntranceVFXTestAccess::Observe(*Service,Gate);
    TestEqual(TEXT("首次快照与生成事件重复时每门仅登记一次"),FDivineBeastsArenaEntranceVFXTestAccess::Gates(*Service),1);
    TestFalse(TEXT("没有真实PrimaryLocalPlayer与内容注册时不提交旋涡"),FDivineBeastsArenaEntranceVFXTestAccess::HasRequests(*Service));
    TestTrue(TEXT("旧拥有者的Active回调不能发布请求"),FDivineBeastsArenaEntranceVFXTestAccess::LateActivationIgnored(*Service));
    Service->Deinitialize();
    TestEqual(TEXT("退出清除门和全部订阅账本"),FDivineBeastsArenaEntranceVFXTestAccess::Gates(*Service),0);
    FDivineBeastsArenaEntranceVFXTestAccess::Observe(*Service,Gate);
    TestEqual(TEXT("退出后的迟到生成事件不能恢复门"),FDivineBeastsArenaEntranceVFXTestAccess::Gates(*Service),0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaEntranceVFXCancellationTest,
    "DivineBeasts.Arena.VFX.EntranceCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaEntranceVFXCancellationTest::RunTest(const FString&)
{
    if(!TestNotNull(TEXT("LocalPlayer真实Within宿主"),GEngine))return false;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    const TStrongObjectPtr<UDivineBeastsArenaEntranceVFXWorldSubsystem> Service(NewObject<UDivineBeastsArenaEntranceVFXWorldSubsystem>(World));
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    const TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> Adapter(NewObject<UGamePlatformPresentationClientSubsystem>(Player.Get()));
    ON_SCOPE_EXIT {Service->Deinitialize();Adapter->Deinitialize();World->DestroyWorld(false);};
    const FGuid Id=FGuid::NewGuid();TArray<FGamePlatformPresentationRequest> Requests;
    Adapter->RegisterProvider(TEXT("Test.CancelObserver"),0,FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest& Request)
    {Requests.Add(Request);return Request.PredictionState==EGamePlatformPresentationPredictionState::Cancelled;}));
    auto* Gate=World->SpawnActor<ADivineBeastsArenaEntrance>();
    FDivineBeastsArenaEntranceVFXTestAccess::Observe(*Service,Gate);
    // 仅构造退出前已有的中立RequestId；没有声称实际Niagara已经创建。
    FDivineBeastsArenaEntranceVFXTestAccess::OwnRequest(*Service,Gate,*Adapter,Id);
    FDivineBeastsArenaEntranceVFXTestAccess::Remove(*Service,Gate);
    FDivineBeastsArenaEntranceVFXTestAccess::Remove(*Service,Gate);
    TestEqual(TEXT("重复移除只取消本人请求一次"),Requests.Num(),1);
    if(Requests.Num()==1)
    {
        TestEqual(TEXT("取消保持原请求身份"),Requests[0].RequestId,Id);
        TestEqual(TEXT("取消沿用同一VFX提供者"),Requests[0].ProviderChannel,FName(TEXT("VFX")));
        TestEqual(TEXT("取消真实终态"),Requests[0].PredictionState,EGamePlatformPresentationPredictionState::Cancelled);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaEntranceVFXRegistrationFailureTest,
    "DivineBeasts.Arena.VFX.EntranceRegistrationFailure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaEntranceVFXRegistrationFailureTest::RunTest(const FString&)
{
    if(!TestNotNull(TEXT("LocalPlayer真实Within宿主"),GEngine))return false;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    const TStrongObjectPtr<UDivineBeastsArenaEntranceVFXWorldSubsystem> Service(NewObject<UDivineBeastsArenaEntranceVFXWorldSubsystem>(World));
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    const TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> Platform(NewObject<UGamePlatformPresentationClientSubsystem>(Player.Get()));
    const TStrongObjectPtr<UDivineBeastsPresentationClientSubsystem> Project(NewObject<UDivineBeastsPresentationClientSubsystem>(Player.Get()));
    ON_SCOPE_EXIT {Service->Deinitialize();Platform->Deinitialize();World->DestroyWorld(false);};
    auto* Gate=World->SpawnActor<ADivineBeastsArenaEntrance>();FDivineBeastsArenaEntranceVFXTestAccess::Observe(*Service,Gate);
    AddExpectedMessagePlain(TEXT("竞技门旋涡内容注册/预载未受理"),ELogVerbosity::Warning,EAutomationExpectedMessageFlags::Contains,1);
    TestTrue(TEXT("真实项目内容服务未初始化时失败关闭；重复通知不循环申请"),FDivineBeastsArenaEntranceVFXTestAccess::RejectUnavailableRegistration(*Service,*Project,*Platform));
    TestEqual(TEXT("失败不留内容包"),Project->GetActiveContentPackCount(),0);
    TestEqual(TEXT("失败不留预载请求"),Project->GetLogicalPreloadCount(),0);
    return true;
}
#endif
