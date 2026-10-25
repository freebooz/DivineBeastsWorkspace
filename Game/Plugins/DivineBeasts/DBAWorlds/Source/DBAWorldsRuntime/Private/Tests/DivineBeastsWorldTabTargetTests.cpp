#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Components/InputComponent.h"
#include "Components/BoxComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/InputSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldTabTargetCycleTest,
    "DivineBeasts.World.Controls.TabTarget.CyclingAndSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsWorldTabTargetCycleTest::RunTest(const FString&)
{
    const FName Action(TEXT("DBA.CycleWorldTarget"));
    TArray<FInputActionKeyMapping> Mappings;
    GetDefault<UInputSettings>()->GetActionMappingByName(Action, Mappings);
    if (!TestEqual(TEXT("TAB只有一个真实动作映射"), Mappings.Num(), 1)) return false;
    TestTrue(TEXT("动作绑定无修饰TAB"), Mappings[0].Key == EKeys::Tab && !Mappings[0].bShift && !Mappings[0].bCtrl && !Mappings[0].bAlt);
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    auto* Controller = World->SpawnActor<ADivineBeastsWorldPlayerController>();
    // 无NetDriver的隔离World也必须拥有真实LocalPlayer；否则UE5.8将控制器视为非本地。
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    Controller->SetPlayer(Player.Get());
    auto* Source = World->SpawnActor<ACharacter>();
    Controller->Possess(Source);
    if (!Controller->InputComponent) Controller->SetupInputComponent();
    FInputActionBinding* Binding = nullptr;
    for (int32 Index = 0; Index < Controller->InputComponent->GetNumActionBindings(); ++Index)
    {
        auto& Entry = Controller->InputComponent->GetActionBinding(Index);
        if (Entry.GetActionName() == Action && Entry.KeyEvent == IE_Pressed) Binding = &Entry;
    }
    if (!TestNotNull(TEXT("真实控制器接收TAB Pressed"), Binding)) return false;
    TestTrue(TEXT("动作消费输入，避免Pawn重复切换"), Binding->bConsumeInput);
    auto Press = [&] { Binding->ActionDelegate.Execute(EKeys::Tab); };
    auto Pawn = [&](FVector Location)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return World->SpawnActor<ACharacter>(ACharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
    };
    ACharacter* First = Pawn(FVector(300., -180., 0.));
    ACharacter* Friendly = Pawn(FVector(600., 180., 0.));
    ACharacter* Last = Pawn(FVector(900., -180., 0.));
    // 无阵营过滤：普通Pawn模拟友方，既不需要敌对标签，也不借输入授予伤害资格。
    ACharacter* Dead = Pawn(FVector(100., 180., 0.));
    auto* Combat = NewObject<UGamePlatformCombatComponent>(Dead);
    Dead->AddInstanceComponent(Combat); Combat->RegisterComponent();
    auto* DeadProperty = FindFProperty<FBoolProperty>(UGamePlatformCombatComponent::StaticClass(), TEXT("bDead"));
    if (!TestNotNull(TEXT("使用真实复制死亡字段夹具"), DeadProperty)) return false;
    DeadProperty->SetPropertyValue_InContainer(Combat, true);
    Pawn(FVector(10001., 0., 0.));
    auto* Hidden = Pawn(FVector(150., -180., 0.)); Hidden->SetActorHiddenInGame(true);
    Pawn(FVector(250., 500., 0.));
    AActor* Occluder = World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Occluder);
    Occluder->SetRootComponent(Box); Occluder->AddInstanceComponent(Box);
    Box->SetBoxExtent(FVector(60., 60., 100.));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore);
    Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Box->RegisterComponent(); Occluder->SetActorLocation(FVector(125., 250., 32.));
    Controller->bWorldInputActive = true;
    Controller->UpdateWorldInputLocks(false);
    int32 Changed = 0;
    const auto Handle = Controller->OnWorldTargetChanged.AddLambda([&Changed] { ++Changed; });
    ON_SCOPE_EXIT { Controller->OnWorldTargetChanged.Remove(Handle); };
    Press(); TestTrue(TEXT("首次选择最近的有效单位"), Controller->GetSelectedWorldTarget() == First);
    Controller->bPointerClickPending = true; Controller->PointerPressedTarget = First;
    Press(); TestTrue(TEXT("第二次包括友方普通Pawn"), Controller->GetSelectedWorldTarget() == Friendly);
    TestFalse(TEXT("TAB撤销旧鼠标点击意图，松键不能回选旧目标"), Controller->bPointerClickPending || Controller->PointerPressedTarget.IsValid());
    Press(); TestTrue(TEXT("继续切换下一目标"), Controller->GetSelectedWorldTarget() == Last);
    Press(); TestTrue(TEXT("到末尾循环回第一目标"), Controller->GetSelectedWorldTarget() == First);
    TestEqual(TEXT("每次真实选择沿用既有变化通知"), Changed, 4);
    Controller->SetWorldGameplayInputSuspended(true);
    const int32 PausedChanges = Changed; Press();
    TestNull(TEXT("菜单暂停清理选择后TAB不能恢复旧目标"), Controller->GetSelectedWorldTarget());
    TestEqual(TEXT("暂停的TAB不发布新选择通知"), Changed, PausedChanges);
    Controller->bWorldGameplayInputSuspended = false; Controller->bWorldInputActive = false; Press();
    TestNull(TEXT("世界未Active时TAB不能恢复旧目标"), Controller->GetSelectedWorldTarget());
    Controller->bWorldInputActive = true;
    Controller->UpdateWorldInputLocks(false);
    First->Destroy(); Friendly->Destroy(); Last->Destroy();
    Press(); TestNull(TEXT("目标销毁后没有候选则清空"), Controller->GetSelectedWorldTarget());
    return true;
}
#endif
