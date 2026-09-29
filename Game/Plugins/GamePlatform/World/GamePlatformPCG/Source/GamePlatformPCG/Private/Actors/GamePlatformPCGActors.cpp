#include "Actors/GamePlatformPCGActors.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "PCGComponent.h"
#include "Schema/GamePlatformPCGSchema.h"

AGamePlatformPCGActorBase::AGamePlatformPCGActorBase()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCGComponent"));
    PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
    PCGComponent->bGenerateOnDropWhenTriggerOnDemand = false;
#if WITH_EDITOR
    PCGComponent->bRegenerateInEditor = false;
#endif
}

void AGamePlatformPCGActorBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (!HasAnyFlags(RF_ClassDefaultObject) && !SourceId.IsValid())
    {
        SourceId = FGuid::NewGuid();
    }

    RequiredSchemaMajor = FMath::Max(1, RequiredSchemaMajor);
}

AGamePlatformPCGVolumeActor::AGamePlatformPCGVolumeActor()
{
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    Bounds->SetupAttachment(SceneRoot);
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetGenerateOverlapEvents(false);
    Bounds->SetCanEverAffectNavigation(false);
}

AGamePlatformPCGSplineActor::AGamePlatformPCGSplineActor()
{
    Primitive = EGamePlatformPCGPrimitive::P2_Linear;
    WorldStage = EGamePlatformPCGWorldStage::Networks;

    Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
    Spline->SetupAttachment(SceneRoot);
}

AGamePlatformPCGPolygonActor::AGamePlatformPCGPolygonActor()
{
    Primitive = EGamePlatformPCGPrimitive::P4_Parcel;
    WorldStage = EGamePlatformPCGWorldStage::Parcels;

    Boundary = CreateDefaultSubobject<USplineComponent>(TEXT("Boundary"));
    Boundary->SetupAttachment(SceneRoot);
    Boundary->SetClosedLoop(true, false);
}

AGamePlatformPCGConnectorActor::AGamePlatformPCGConnectorActor()
{
    Primitive = EGamePlatformPCGPrimitive::P3_Connector;
    WorldStage = EGamePlatformPCGWorldStage::Connectors;
}

AGamePlatformPCGExclusionActor::AGamePlatformPCGExclusionActor()
{
    WorldStage = EGamePlatformPCGWorldStage::FieldRead;
    Primitive = EGamePlatformPCGPrimitive::P0_Field;
}

AGamePlatformPCGWorldDirector::AGamePlatformPCGWorldDirector()
{
    PrimaryActorTick.bCanEverTick = false;
}

bool AGamePlatformPCGWorldDirector::RegisterParticipant(AGamePlatformPCGActorBase* Participant)
{
    if (!IsValid(Participant) || Participant->GetWorld() != GetWorld() || Participants.Contains(Participant))
    {
        return false;
    }

    Participants.Add(Participant);
    return true;
}

bool AGamePlatformPCGWorldDirector::UnregisterParticipant(AGamePlatformPCGActorBase* Participant)
{
    return IsValid(Participant) && Participants.Remove(Participant) > 0;
}

TArray<AGamePlatformPCGActorBase*> AGamePlatformPCGWorldDirector::GetParticipantsForStage(EGamePlatformPCGWorldStage Stage) const
{
    TArray<AGamePlatformPCGActorBase*> Result;
    for (AGamePlatformPCGActorBase* Participant : Participants)
    {
        if (IsValid(Participant) && Participant->WorldStage == Stage)
        {
            Result.Add(Participant);
        }
    }

    Result.Sort(
        [](const AGamePlatformPCGActorBase& Left, const AGamePlatformPCGActorBase& Right)
        {
            return Left.SourceId < Right.SourceId;
        });
    return Result;
}

bool AGamePlatformPCGWorldDirector::ValidateParticipantSet(FString& OutError) const
{
    OutError.Reset();
    TSet<FGuid> SourceIds;

    for (const AGamePlatformPCGActorBase* Participant : Participants)
    {
        if (!IsValid(Participant) || Participant->GetWorld() != GetWorld())
        {
            OutError = TEXT("WorldDirector包含无效或跨World的PCG参与者。");
            return false;
        }

        if (!Participant->SourceId.IsValid() || SourceIds.Contains(Participant->SourceId))
        {
            OutError = TEXT("PCG参与者SourceId无效或重复。");
            return false;
        }

        if (Participant->RequiredSchemaMajor != FGamePlatformPCGSchema::CurrentVersion().Major)
        {
            OutError = TEXT("PCG参与者要求的Schema主版本与平台当前版本不一致。");
            return false;
        }

        SourceIds.Add(Participant->SourceId);
    }

    return true;
}
