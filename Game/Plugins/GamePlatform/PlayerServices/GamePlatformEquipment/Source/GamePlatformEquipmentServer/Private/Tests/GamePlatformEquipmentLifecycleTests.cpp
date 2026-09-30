// 装备组件退出回归：ASC保持存活，移除组件只撤销本组件授予，迟到落库回调不能重新授予。
#include "Components/GamePlatformEquipmentServerComponent.h"
#include "Components/GamePlatformEquipmentComponent.h"
#include "Definitions/GamePlatformEquipmentDefinition.h"
#include "Interfaces/GamePlatformEquipmentPersistencePort.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
// 仅测试手动完成的Port，无磁盘/HTTP，不作为生产持久化实现。
class FEquipmentLifecyclePersistence final : public IGamePlatformEquipmentPersistencePort
{
public:
    FGamePlatformEquipmentLoadCompletion Pending;
    bool BeginLoadEquipment(const FString&, const FString&, FGamePlatformEquipmentLoadCompletion Completion) override { Pending = MoveTemp(Completion); return true; }
    bool BeginEquip(const FString&, const FGamePlatformEquipRequest&, FGamePlatformEquipmentMutationCompletion) override { return false; }
    bool BeginUnequip(const FString&, const FGamePlatformUnequipRequest&, FGamePlatformEquipmentMutationCompletion) override { return false; }
    bool BeginQueryOperationResult(const FString&, const FString&, const FGuid&, FGamePlatformEquipmentMutationCompletion) override { return false; }
};
class FEquipmentLifecycleResolver final : public IGamePlatformEquipmentGameplayAssetResolver
{
public:
    bool ResolveAbilitySet(FName, TArray<TSubclassOf<UGameplayAbility>>& Abilities, TArray<TSubclassOf<UGameplayEffect>>&) override { Abilities.Add(UGameplayAbility::StaticClass()); return true; }
    bool ResolveGameplayEffect(FName, TSubclassOf<UGameplayEffect>&) override { return false; }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEquipmentComponentRemovalTest, "GamePlatform.Equipment.Server.ComponentRemoval", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEquipmentComponentRemovalTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) { AddError(TEXT("临时权威世界创建失败")); return false; }
    World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
    AActor* Owner = World->SpawnActor<AActor>();
    auto* ASC = NewObject<UAbilitySystemComponent>(Owner); ASC->RegisterComponent(); ASC->InitAbilityActorInfo(Owner, Owner);
    auto* State = NewObject<UGamePlatformEquipmentComponent>(Owner); State->RegisterComponent();
    auto* Runtime = NewObject<UGamePlatformEquipmentServerComponent>(Owner); Runtime->RegisterComponent();
    auto* Definition = NewObject<UGamePlatformEquipmentDefinition>();
    Definition->EquipmentDefinitionId = TEXT("Equipment"); Definition->CompatibleItemDefinitionId = TEXT("Item");
    Definition->AllowedSlotIds.Add(TEXT("Hand")); Definition->AbilitySetDefinitionIds.Add(TEXT("AbilitySet"));
    Runtime->RegisterDefinition(Definition);
    auto Persistence = MakeShared<FEquipmentLifecyclePersistence, ESPMode::ThreadSafe>();
    auto Resolver = MakeShared<FEquipmentLifecycleResolver, ESPMode::ThreadSafe>();
    auto Grant = MakeShared<FGamePlatformEquipmentGASGrantPort, ESPMode::ThreadSafe>(Resolver);
    TestTrue(TEXT("初始化接纳真实运行Port"), Runtime->InitializeEquipmentRuntime(TEXT("Player"), TEXT("Character"), State, Persistence, Grant));
    Runtime->BindAvatar(ASC, 1);
    FGamePlatformEquipmentSnapshot Snapshot; Snapshot.CharacterId = TEXT("Character"); Snapshot.EquipmentRevision = 1;
    FGamePlatformEquipmentSlotState Slot; Slot.SlotId = TEXT("Hand"); Slot.ItemDefinitionId = TEXT("Item"); Slot.EquipmentDefinitionId = TEXT("Equipment"); Snapshot.Slots.Add(Slot);
    auto Loaded = MoveTemp(Persistence->Pending); Loaded(Snapshot, EGamePlatformEquipmentError::None);
    TestEqual(TEXT("组件拥有一项GAS授予"), ASC->GetActivatableAbilities().Num(), 1);
    Runtime->Reconcile(); auto Late = MoveTemp(Persistence->Pending);
    Runtime->DestroyComponent();
    TestEqual(TEXT("ASC存活但组件授予已经撤销"), ASC->GetActivatableAbilities().Num(), 0);
    Late(Snapshot, EGamePlatformEquipmentError::None);
    TestEqual(TEXT("退出后迟到回调不能重新授予"), ASC->GetActivatableAbilities().Num(), 0);
    World->DestroyWorld(false);
    return true;
}
#endif
