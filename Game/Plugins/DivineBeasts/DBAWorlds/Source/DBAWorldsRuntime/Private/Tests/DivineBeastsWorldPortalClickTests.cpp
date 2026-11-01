#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Gameplay/DivineBeastsArenaEntrance.h"
#include "Definitions/DivineBeastsArenaEntranceDefinition.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

// 缺少点击到平台交互的调用时失败；不伪造匹配、后端准入或跨网成功。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldPortalClickTest,
    "DivineBeasts.World.Controls.PortalClick.DispatchAndSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsWorldPortalClickTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
    UWorld* World=Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    auto* Controller=World->SpawnActor<ADivineBeastsWorldPlayerController>();
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    Controller->SetPlayer(Player.Get());
    auto* Source=World->SpawnActor<ACharacter>(); Controller->Possess(Source);
    Controller->DispatchBeginPlay();
    Controller->bWorldInputActive=true; Controller->UpdateWorldInputLocks(false);
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Gate=World->SpawnActor<ADivineBeastsArenaEntrance>(ADivineBeastsArenaEntrance::StaticClass(),FVector(200.,0.,-100.),FRotator::ZeroRotator,Spawn);
    auto* Definition=NewObject<UDivineBeastsArenaEntranceDefinition>(Gate);
    FGamePlatformId::TryParse(TEXT("divinebeasts.activityentrance.clicktest@1"),Definition->LogicalId);
    Definition->ArenaModeId=TEXT("Arena.Mode.Duel1v1"); Gate->EntranceDefinition=Definition;
    Gate->OnConstruction(Gate->GetActorTransform()); Gate->DispatchBeginPlay();
    auto* Interactor=Controller->FindComponentByClass<UGamePlatformInteractorComponent>();
    // 夹具没有桌面指针，使用组件已有的镜头焦点模式；点击对象由真实控制器入口接收。
    Interactor->SetLocalFocusPolicy(true,false); Interactor->RefreshLocalFocus();
    if(!TestTrue(TEXT("真实平台射线已聚焦同一个250厘米内的门"),Interactor->GetCurrentFocus().TargetActor==Gate&&Interactor->GetCurrentFocus().bLocallyAvailable))return false;
    Controller->CommitWorldPointerClick(Gate);
    const FGuid Request=Interactor->GetLastResult().RequestId;
    TestTrue(TEXT("点门必须向平台发出真实交互请求，不能只选中"),Request.IsValid());
    TestEqual(TEXT("普通测试角色没有服务器Active资格，点击仍由权威拒绝"),Interactor->GetLastResult().Error,EGamePlatformInteractionError::InteractorNotActive);
    TestTrue(TEXT("门户不会被当作战斗选中目标"),Controller->GetSelectedWorldTarget()!=Gate);
    auto* Other=World->SpawnActor<ACharacter>(); Other->SetActorLocation(FVector(0.,400.,0.));
    Controller->CommitWorldPointerClick(Other);
    TestTrue(TEXT("普通单位仍使用原有目标选择"),Controller->GetSelectedWorldTarget()==Other);
    TestEqual(TEXT("选择单位不发起门交互"),Interactor->GetLastResult().RequestId,Request);
    auto* OtherGate=World->SpawnActor<ADivineBeastsArenaEntrance>(ADivineBeastsArenaEntrance::StaticClass(),FVector(200.,300.,-100.),FRotator::ZeroRotator,Spawn);
    OtherGate->EntranceDefinition=Definition; OtherGate->OnConstruction(OtherGate->GetActorTransform()); OtherGate->DispatchBeginPlay();
    Controller->CommitWorldPointerClick(OtherGate);
    TestEqual(TEXT("点击另一扇门不能沿旧焦点误提交当前门"),Interactor->GetLastResult().RequestId,Request);
    Source->SetActorLocation(FVector(600.,0.,0.));
    Controller->CommitWorldPointerClick(Gate);
    TestEqual(TEXT("角色超出交互距离不能因可见而提交"),Interactor->GetLastResult().RequestId,Request);
    Source->SetActorLocation(FVector::ZeroVector);
    auto* Camera=World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),FVector(-1800.,0.,64.),FRotator::ZeroRotator,Spawn);
    Controller->SetViewTarget(Camera);
    if(!TestNotNull(TEXT("真实控制器拥有镜头管理器"),Controller->PlayerCameraManager.Get()))return false;
    Controller->PlayerCameraManager->UpdateCamera(0.f);
    FVector ViewLocation; FRotator ViewRotation; Controller->GetPlayerViewPoint(ViewLocation,ViewRotation);
    if(!TestTrue(TEXT("第三人称夹具镜头距离确实超过六米"),FVector::Distance(ViewLocation,Source->GetActorLocation())>600.))return false;
    Interactor->RefreshLocalFocus();
    TestTrue(TEXT("18米观察镜头仍能发现角色250厘米内的同一门"),Interactor->GetCurrentFocus().TargetActor==Gate&&Interactor->GetCurrentFocus().bLocallyAvailable);
    Source->SetActorLocation(FVector(600.,0.,0.));
    Interactor->RefreshLocalFocus();
    TestFalse(TEXT("补偿镜头射线不会放宽角色到门的真实交互距离"),Interactor->GetCurrentFocus().bLocallyAvailable);
    Source->SetActorLocation(FVector::ZeroVector);
    Gate->FindComponentByClass<UGamePlatformInteractableComponent>()->SetInteractionEnabled(false);
    Controller->CommitWorldPointerClick(Gate);
    TestEqual(TEXT("禁用的门不发起新请求"),Interactor->GetLastResult().RequestId,Request);
    Gate->FindComponentByClass<UGamePlatformInteractableComponent>()->SetInteractionEnabled(true);
    Controller->SetSelectedWorldTarget(nullptr);
    Controller->bWorldGameplayInputSuspended=true;
    Controller->CommitWorldPointerClick(Gate);
    TestEqual(TEXT("暂停后点击不能发起请求"),Interactor->GetLastResult().RequestId,Request);
    Controller->CommitWorldPointerClick(Other);
    TestNull(TEXT("界面暂停时也不能改普通选择"),Controller->GetSelectedWorldTarget());
    return true;
}
#endif
