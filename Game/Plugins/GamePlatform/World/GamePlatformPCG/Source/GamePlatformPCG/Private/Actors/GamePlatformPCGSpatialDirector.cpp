#include "Actors/GamePlatformPCGActors.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "PCGComponent.h"
#include "Services/GamePlatformPCGSpatialRules.h"

namespace
{
/** 将旋转/缩放后的盒体XY轮廓固定为4个顶点，严禁把碰撞/导航状态写回原Actor。 */
bool AppendBoxFootprint(const FTransform& WorldTransform, const FVector& UnscaledHalfExtent,
    FGamePlatformPCGSpatialMask& Mask)
{
    if (UnscaledHalfExtent.X <= 0.0 || UnscaledHalfExtent.Y <= 0.0 ||
        !FMath::IsFinite(UnscaledHalfExtent.X) || !FMath::IsFinite(UnscaledHalfExtent.Y))
    {
        return false;
    }
    for (const FVector2D Corner : {FVector2D(-1, -1), FVector2D(1, -1),
        FVector2D(1, 1), FVector2D(-1, 1)})
    {
        const FVector Position = WorldTransform.TransformPosition(
            FVector(Corner.X * UnscaledHalfExtent.X, Corner.Y * UnscaledHalfExtent.Y, 0.0));
        Mask.Vertices.Add(FVector2D(Position.X, Position.Y));
    }
    Mask.bClosed = true;
    return true;
}

/**
 * 按世界厘米长度等距采样UE样条，保留弯道的真实几何形状而不是直接连接稀疏控制点。
 * 限制最多1024个点；对异常长样条失败关闭而不是悄悄降低精度或越界分配。
 */
bool AppendSplineVertices(const USplineComponent& Spline, bool bClosed,
    FGamePlatformPCGSpatialMask& Mask)
{
    const double Length = Spline.GetSplineLength();
    if (!FMath::IsFinite(Length) || Length < 1.0)
    {
        return false;
    }
    const int32 Segments = FMath::Max(1, FMath::CeilToInt(Length / 200.0));
    const int32 Count = bClosed ? Segments : Segments + 1;
    if (Count > 1024 || (bClosed && Count < 3))
    {
        return false;
    }
    Mask.bClosed = bClosed;
    Mask.Vertices.Reserve(Count);
    for (int32 I = 0; I < Count; ++I)
    {
        const float Distance = static_cast<float>((static_cast<double>(I) / Segments) * Length);
        const FVector P = Spline.GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
        Mask.Vertices.Add(FVector2D(P.X, P.Y));
    }
    return true;
}
}

bool AGamePlatformPCGWorldDirector::CollectSpatialMasks(
    TArray<FGamePlatformPCGSpatialMask>& OutMasks, FString& OutError) const
{
    check(IsInGameThread());
    OutMasks.Reset();
    if (!ValidateParticipantSet(OutError))
    {
        return false;
    }

    for (AGamePlatformPCGActorBase* Participant : Participants)
    {
        if (!IsValid(Participant)) { OutError = TEXT("PCG放置器失效，拒绝构造不完整空间快照。"); OutMasks.Reset(); return false; }
        FGamePlatformPCGSpatialMask Mask;
        Mask.SourceId = Participant->SourceId;

        if (const AGamePlatformPCGExclusionActor* Exclusion = Cast<AGamePlatformPCGExclusionActor>(Participant))
        {
            Mask.Priority = Exclusion->CarvePriority;
            Mask.Strength = Exclusion->Strength;
            if (!Exclusion->Bounds || !AppendBoxFootprint(Exclusion->Bounds->GetComponentTransform(),
                    Exclusion->Bounds->GetUnscaledBoxExtent(), Mask))
            {
                OutError = TEXT("人工排除体积无效，无法生成空间掩码。");
                OutMasks.Reset();
                return false;
            }
        }
        else if (const AGamePlatformPCGSplineActor* Road = Cast<AGamePlatformPCGSplineActor>(Participant))
        {
            if (!Road->bExportsSpatialMask) { continue; }
            Mask.Priority = Road->CarvePriority;
            Mask.HalfWidthCm = Road->CarveHalfWidthCm;
            if (!Road->Spline || !AppendSplineVertices(*Road->Spline, Road->Spline->IsClosedLoop(), Mask))
            {
                OutError = TEXT("道路线性样条为空、异常或超出1024采样点上限。");
                OutMasks.Reset();
                return false;
            }
        }
        else if (const AGamePlatformPCGPolygonActor* Parcel = Cast<AGamePlatformPCGPolygonActor>(Participant))
        {
            if (!Parcel->bExportsSpatialMask) { continue; }
            Mask.Priority = Parcel->CarvePriority;
            if (!Parcel->Boundary || !Parcel->Boundary->IsClosedLoop() ||
                !AppendSplineVertices(*Parcel->Boundary, true, Mask))
            {
                OutError = TEXT("农业/院落地块边界必须是有效闭合多边形。");
                OutMasks.Reset();
                return false;
            }
        }
        else if (const AGamePlatformPCGConnectorActor* Connector = Cast<AGamePlatformPCGConnectorActor>(Participant))
        {
            Mask.Priority = Connector->CarvePriority;
            const double R = Connector->CarveRadiusCm;
            if (!FMath::IsFinite(R) || R <= 0.0)
            {
                OutError = TEXT("连接件空间占位半径无效。");
                OutMasks.Reset();
                return false;
            }
            if (!AppendBoxFootprint(Connector->GetActorTransform(), FVector(R, R, 0.0), Mask))
            {
                OutError = TEXT("连接件不能生成有效占位轮廓。");
                OutMasks.Reset();
                return false;
            }
        }
        else
        {
            continue;
        }

        if (!FGamePlatformPCGSpatialRules::ValidateMask(Mask) || OutMasks.Num() >= 256)
        {
            OutError = TEXT("空间Mask来源无效、优先级越界或超出单次256来源上限。");
            OutMasks.Reset();
            return false;
        }
        OutMasks.Add(MoveTemp(Mask));
    }

    OutMasks.Sort([](const FGamePlatformPCGSpatialMask& A, const FGamePlatformPCGSpatialMask& B)
    {
        if (A.Priority != B.Priority) { return A.Priority > B.Priority; }
        return A.SourceId.ToString(EGuidFormats::Digits) < B.SourceId.ToString(EGuidFormats::Digits);
    });
    OutError.Reset();
    return true;
}

bool AGamePlatformPCGWorldDirector::BuildStaticExecutionPlan(
    TArray<AGamePlatformPCGActorBase*>& OutPlan, FString& OutError) const
{
    check(IsInGameThread());
    OutPlan.Reset();
    if (!ValidateParticipantSet(OutError))
    {
        return false;
    }

    for (AGamePlatformPCGActorBase* Participant : Participants)
    {
        // 仅组织顺序和合同，不主动调用Generate；不得把禁用阶段隐式执行为成功。
        const EGamePlatformPCGWorldStage Stage = Participant->WorldStage;
        if (Stage == EGamePlatformPCGWorldStage::TerrainWrite ||
            Stage == EGamePlatformPCGWorldStage::CutFillRequest ||
            Stage == EGamePlatformPCGWorldStage::Interiors ||
            Stage == EGamePlatformPCGWorldStage::ApplyState ||
            Stage == EGamePlatformPCGWorldStage::RuntimeDetail)
        {
            OutError = TEXT("Editor Static M0/M1不支持TerrainWrite、CutFill、Interiors、ApplyState或RuntimeDetail。");
            OutPlan.Reset();
            return false;
        }
        if (!IsValid(Participant->PCGComponent) || Participant->Graph.IsNull())
        {
            OutError = TEXT("PCG放置器缺少实际原生组件或图引用，不能进入静态生成计划。");
            OutPlan.Reset();
            return false;
        }
        OutPlan.Add(Participant);
    }
    OutPlan.Sort([](const AGamePlatformPCGActorBase& A, const AGamePlatformPCGActorBase& B)
    {
        const int32 StageA = static_cast<int32>(A.WorldStage);
        const int32 StageB = static_cast<int32>(B.WorldStage);
        return StageA == StageB ? A.SourceId < B.SourceId : StageA < StageB;
    });
    OutError.Reset();
    return true;
}
