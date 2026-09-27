#include "Components/GamePlatformEquipmentVisualComponent.h"

#include "Components/GamePlatformEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Definitions/GamePlatformEquipmentVisualDefinition.h"
#include "Loading/GamePlatformAssetLoader.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

UGamePlatformEquipmentVisualComponent::UGamePlatformEquipmentVisualComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformEquipmentVisualComponent::RegisterVisualDefinition(
    UGamePlatformEquipmentVisualDefinition* Definition)
{
    if (!IsValid(Definition) ||
        Definition->EquipmentVisualId.IsNone())
    {
        return;
    }

    Definitions.Add(
        Definition->EquipmentVisualId,
        Definition);
}

void UGamePlatformEquipmentVisualComponent::BindEquipmentComponent(
    UGamePlatformEquipmentComponent* InEquipmentComponent)
{
    if (UGamePlatformEquipmentComponent* Existing =
        EquipmentComponent.Get())
    {
        Existing->OnPublicEquipmentChanged.RemoveAll(this);
    }

    EquipmentComponent = InEquipmentComponent;

    if (IsValid(InEquipmentComponent))
    {
        InEquipmentComponent->OnPublicEquipmentChanged.AddUObject(
            this,
            &UGamePlatformEquipmentVisualComponent::RefreshVisuals);
    }

    RefreshVisuals();
}

void UGamePlatformEquipmentVisualComponent::BindAvatar(
    USkeletalMeshComponent* InAvatarMesh,
    int32 InAvatarGeneration)
{
    AvatarMesh = InAvatarMesh;
    AvatarGeneration = FMath::Max(0, InAvatarGeneration);
    ++VisualRequestGeneration;
    RefreshVisuals();
}

void UGamePlatformEquipmentVisualComponent::RefreshVisuals()
{
    ++VisualRequestGeneration;
    ClearVisuals();

    UGamePlatformEquipmentComponent* Equipment =
        EquipmentComponent.Get();
    USkeletalMeshComponent* Avatar = AvatarMesh.Get();

    if (!IsValid(Equipment) || !IsValid(Avatar))
    {
        return;
    }

    const uint64 RequestGeneration = VisualRequestGeneration;
    const int32 ExpectedAvatarGeneration = AvatarGeneration;

    for (const FGamePlatformPublicEquipmentSlotState& Slot :
         Equipment->GetPublicSnapshot().Slots)
    {
        UGamePlatformEquipmentVisualDefinition* Definition =
            Definitions.FindRef(Slot.VisualDefinitionId);

        if (!IsValid(Definition) ||
            Definition->VisualType !=
                EGamePlatformEquipmentVisualType::StaticMesh)
        {
            continue;
        }

        RequestStaticMesh(
            Slot.SlotId,
            Definition,
            RequestGeneration,
            ExpectedAvatarGeneration);
    }
}

void UGamePlatformEquipmentVisualComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (UGamePlatformEquipmentComponent* Equipment =
        EquipmentComponent.Get())
    {
        Equipment->OnPublicEquipmentChanged.RemoveAll(this);
    }

    ++VisualRequestGeneration;
    ClearVisuals();
    Super::EndPlay(EndPlayReason);
}

void UGamePlatformEquipmentVisualComponent::ClearVisuals()
{
    for (TPair<FName, TSharedPtr<FStreamableHandle>>& Pair :
         ActiveLoadHandles)
    {
        FGamePlatformAssetLoader::Cancel(Pair.Value);
    }
    ActiveLoadHandles.Reset();

    for (TPair<FName, TObjectPtr<UStaticMeshComponent>>& Pair :
         ActiveStaticMeshes)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->DestroyComponent();
        }
    }

    ActiveStaticMeshes.Reset();
}

void UGamePlatformEquipmentVisualComponent::RequestStaticMesh(
    FName SlotId,
    UGamePlatformEquipmentVisualDefinition* Definition,
    uint64 ExpectedRequestGeneration,
    int32 ExpectedAvatarGeneration)
{
    if (SlotId.IsNone() ||
        !IsValid(Definition) ||
        Definition->StaticMesh.IsNull())
    {
        return;
    }

    TArray<FSoftObjectPath> Assets;
    Assets.Add(Definition->StaticMesh.ToSoftObjectPath());

    for (const TSoftObjectPtr<UMaterialInterface>& Material :
         Definition->MaterialOverrides)
    {
        if (!Material.IsNull())
        {
            Assets.Add(Material.ToSoftObjectPath());
        }
    }

    TWeakObjectPtr<UGamePlatformEquipmentVisualComponent> WeakThis(this);
    TWeakObjectPtr<UGamePlatformEquipmentVisualDefinition> WeakDefinition(
        Definition);

    TSharedPtr<FStreamableHandle> Handle =
        FGamePlatformAssetLoader::RequestAsyncLoad(
            Assets,
            FStreamableDelegate::CreateLambda(
            [WeakThis,
             WeakDefinition,
             SlotId,
             ExpectedRequestGeneration,
             ExpectedAvatarGeneration]()
            {
                UGamePlatformEquipmentVisualComponent* Self =
                    WeakThis.Get();
                UGamePlatformEquipmentVisualDefinition* Visual =
                    WeakDefinition.Get();

                if (!Self ||
                    !IsValid(Visual) ||
                    Self->VisualRequestGeneration !=
                        ExpectedRequestGeneration ||
                    Self->AvatarGeneration !=
                        ExpectedAvatarGeneration)
                {
                    return;
                }

                USkeletalMeshComponent* Avatar =
                    Self->AvatarMesh.Get();
                UStaticMesh* Mesh = Visual->StaticMesh.Get();

                if (!IsValid(Avatar) || !IsValid(Mesh))
                {
                    return;
                }

                if (!Visual->SocketName.IsNone() &&
                    !Avatar->DoesSocketExist(Visual->SocketName))
                {
                    return;
                }

                AActor* Owner = Self->GetOwner();
                if (!IsValid(Owner))
                {
                    return;
                }

                UStaticMeshComponent* Component =
                    NewObject<UStaticMeshComponent>(
                        Owner,
                        NAME_None,
                        RF_Transient);

                if (!IsValid(Component))
                {
                    return;
                }

                Owner->AddInstanceComponent(Component);
                Component->SetStaticMesh(Mesh);

                for (int32 Index = 0;
                     Index < Visual->MaterialOverrides.Num();
                     ++Index)
                {
                    if (UMaterialInterface* Material =
                        Visual->MaterialOverrides[Index].Get())
                    {
                        Component->SetMaterial(Index, Material);
                    }
                }

                Component->RegisterComponent();
                Component->AttachToComponent(
                    Avatar,
                    FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                    Visual->SocketName);
                Component->SetRelativeTransform(
                    Visual->RelativeTransform);

                Self->ActiveStaticMeshes.Add(
                    SlotId,
                    Component);
                Self->ActiveLoadHandles.Remove(SlotId);
            }));

    if (Handle.IsValid())
    {
        ActiveLoadHandles.Add(SlotId, MoveTemp(Handle));
    }
}
