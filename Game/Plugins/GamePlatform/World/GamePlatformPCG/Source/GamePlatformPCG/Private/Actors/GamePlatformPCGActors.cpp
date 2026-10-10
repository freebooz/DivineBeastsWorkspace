#include "Actors/GamePlatformPCGActors.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "Schema/GamePlatformPCGSchema.h"

AGamePlatformPCGActorBase::AGamePlatformPCGActorBase()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    // 所有平台PCG放置器均须具备有效原生空间包围盒：
    // Spline/Polygon/Connector本身没有UPrimitiveComponent，不能依赖视觉网格或玩法碰撞。
    GenerationBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("GenerationBounds"));
    GenerationBounds->SetupAttachment(SceneRoot);
    GenerationBounds->SetBoxExtent(FVector(250.0f, 250.0f, 200.0f), false);
    GenerationBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GenerationBounds->SetGenerateOverlapEvents(false);
    GenerationBounds->SetCanEverAffectNavigation(false);

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
#if WITH_EDITOR
    // Editor地编放置器持有经审批的Graph软引用；已加载后必须向官方PCGComponent设置本地图。
    // 使用SetGraphLocal而不是NetMulticast SetGraph，不触发任何网络复制；只绑定，不调用Generate。
    // 服务器Cook/运行期仍只消费已验收的静态成果，避免未授权的运行时装饰生成。
    if (!HasAnyFlags(RF_ClassDefaultObject) && IsValid(PCGComponent))
    {
        UPCGGraph* LoadedGraph = Graph.Get();
        if (LoadedGraph && PCGComponent->GetGraph() != LoadedGraph)
        {
            PCGComponent->SetGraphLocal(LoadedGraph);
        }
    }
#endif
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
