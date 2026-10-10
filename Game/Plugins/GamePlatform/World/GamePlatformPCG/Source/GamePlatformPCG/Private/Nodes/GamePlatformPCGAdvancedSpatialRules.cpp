#include "Services/GamePlatformPCGAdvancedSpatialRules.h"

namespace
{
/** 与主地形无关的纯二维线段求交；平行、重合、不在有限线段内的情况均拒绝。 */
bool SegmentIntersection(const FVector2D& A, const FVector2D& B,
    const FVector2D& C, const FVector2D& D, FVector2D& Out)
{
    const FVector2D R = B - A;
    const FVector2D S = D - C;
    const double Den = R.X * S.Y - R.Y * S.X;
    if (!FMath::IsFinite(Den) || FMath::Abs(Den) < 0.000001)
    {
        return false;
    }
    const FVector2D Delta = C - A;
    const double T = (Delta.X * S.Y - Delta.Y * S.X) / Den;
    const double U = (Delta.X * R.Y - Delta.Y * R.X) / Den;
    if (T < 0.0 || T > 1.0 || U < 0.0 || U > 1.0)
    {
        return false;
    }
    Out = A + R * T;
    return FMath::IsFinite(Out.X) && FMath::IsFinite(Out.Y);
}
}

bool FGamePlatformPCGAdvancedSpatialRules::FindBridgeCandidates(
    const FGamePlatformPCGSpatialMask& Road, const FGamePlatformPCGSpatialMask& Water,
    float AdditionalMarginCm, TArray<FGamePlatformPCGBridgeCandidate>& OutCandidates)
{
    OutCandidates.Reset();
    if (!FGamePlatformPCGSpatialRules::ValidateMask(Road) ||
        !FGamePlatformPCGSpatialRules::ValidateMask(Water) ||
        Road.bClosed || Water.bClosed || Road.Vertices.Num() > 256 ||
        Water.Vertices.Num() > 256 || !FMath::IsFinite(AdditionalMarginCm) ||
        AdditionalMarginCm < 0.0f || AdditionalMarginCm > 10000.0f)
    {
        return false;
    }

    // 分段枚举有界、纯候选且稳定。道路弯线或河流转弯可形成多个交叉；
    // 太多交点通常说明几何错误而不是有效桥梁布局，应直接拒绝。
    for (int32 I = 0; I + 1 < Road.Vertices.Num(); ++I)
    {
        for (int32 J = 0; J + 1 < Water.Vertices.Num(); ++J)
        {
            FVector2D Intersection;
            if (!SegmentIntersection(Road.Vertices[I], Road.Vertices[I + 1],
                    Water.Vertices[J], Water.Vertices[J + 1], Intersection))
            {
                continue;
            }
            bool bDuplicate = false;
            for (const FGamePlatformPCGBridgeCandidate& Existing : OutCandidates)
            {
                if (FVector2D::DistSquared(Existing.IntersectionXY, Intersection) <= 1.0)
                {
                    bDuplicate = true;
                    break;
                }
            }
            if (bDuplicate) { continue; }
            if (OutCandidates.Num() >= 64)
            {
                OutCandidates.Reset();
                return false;
            }

            FGamePlatformPCGBridgeCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
            Candidate.IntersectionXY = Intersection;
            Candidate.RoadSegmentIndex = I;
            Candidate.WaterSegmentIndex = J;
            Candidate.RequiredSpanCm = FMath::Max(1.0f,
                2.0f * Water.HalfWidthCm + 2.0f * AdditionalMarginCm);
            Candidate.bRequiresHumanApproval = true;
        }
    }
    return true;
}

bool FGamePlatformPCGAdvancedSpatialRules::IsInsideCavity(
    const FGamePlatformPCGCavityExclusion& Cavity, const FVector& Position, bool& bOutInside)
{
    bOutInside = false;
    if (!FGamePlatformPCGSpatialRules::ValidateMask(Cavity.Footprint) ||
        !Cavity.Footprint.bClosed ||
        !FMath::IsFinite(Cavity.MinZCm) || !FMath::IsFinite(Cavity.MaxZCm) ||
        Cavity.MinZCm >= Cavity.MaxZCm || !FMath::IsFinite(Position.X) ||
        !FMath::IsFinite(Position.Y) || !FMath::IsFinite(Position.Z))
    {
        return false;
    }

    bOutInside = Position.Z >= Cavity.MinZCm && Position.Z <= Cavity.MaxZCm &&
        FGamePlatformPCGSpatialRules::ContainsPoint(Cavity.Footprint, FVector2D(Position.X, Position.Y));
    return true;
}

bool FGamePlatformPCGAdvancedSpatialRules::ValidateSpatialGraph(
    TConstArrayView<FGamePlatformPCGSpaceNode> Nodes,
    TConstArrayView<FGamePlatformPCGSpaceEdge> Edges, FName EntryId, FName ExitId)
{
    if (Nodes.IsEmpty() || Nodes.Num() > 64 || Edges.Num() > 128 ||
        EntryId.IsNone() || ExitId.IsNone() || EntryId == ExitId)
    {
        return false;
    }
    TMap<FName, int32> Index;
    for (int32 I = 0; I < Nodes.Num(); ++I)
    {
        const FGamePlatformPCGSpaceNode& Node = Nodes[I];
        if (Node.NodeId.IsNone() || Index.Contains(Node.NodeId) || Node.CenterCm.ContainsNaN() ||
            !FMath::IsFinite(Node.ClearanceCm) || Node.ClearanceCm < 100.0f)
        {
            return false;
        }
        Index.Add(Node.NodeId, I);
    }
    if (!Index.Contains(EntryId) || !Index.Contains(ExitId))
    {
        return false;
    }

    TArray<TArray<int32>> Adjacency;
    Adjacency.SetNum(Nodes.Num());
    for (const FGamePlatformPCGSpaceEdge& Edge : Edges)
    {
        const int32* From = Index.Find(Edge.FromNodeId);
        const int32* To = Index.Find(Edge.ToNodeId);
        if (!From || !To || *From == *To || Adjacency[*From].Contains(*To))
        {
            return false;
        }
        Adjacency[*From].Add(*To);
        Adjacency[*To].Add(*From);
    }

    TSet<int32> Visited;
    TArray<int32> Queue = {*Index.Find(EntryId)};
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        const int32 Current = Queue[Head];
        if (Visited.Contains(Current)) { continue; }
        Visited.Add(Current);
        for (const int32 Neighbor : Adjacency[Current])
        {
            if (!Visited.Contains(Neighbor)) { Queue.Add(Neighbor); }
        }
    }
    // 所有房间/通道必须从入口可达，出口必须包含在其中。
    return Visited.Num() == Nodes.Num() && Visited.Contains(*Index.Find(ExitId));
}
