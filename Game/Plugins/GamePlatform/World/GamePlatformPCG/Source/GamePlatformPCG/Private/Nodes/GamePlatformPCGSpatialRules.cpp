#include "Services/GamePlatformPCGSpatialRules.h"

#include "Services/GamePlatformPCGPriorityRules.h"

namespace
{
constexpr int32 MaxVertices = 1024;
constexpr double GeometryEpsilon = 0.000001;

/** 计算点到线段的平方距离，退化线段按端点处理；先校验有限输入。 */
double SegmentDistanceSquared(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
{
    const FVector2D Direction = B - A;
    const double LengthSquared = Direction.SizeSquared();
    if (LengthSquared <= GeometryEpsilon)
    {
        return FVector2D::DistSquared(Point, A);
    }
    const double T = FMath::Clamp(FVector2D::DotProduct(Point - A, Direction) / LengthSquared, 0.0, 1.0);
    return FVector2D::DistSquared(Point, A + Direction * T);
}

/** 经典奇偶射线；点在多边形边界时交给距离判定，避免射线端点数值二义性。 */
bool InsidePolygon(const TArray<FVector2D>& Vertices, const FVector2D& Point)
{
    bool bInside = false;
    for (int32 I = 0, J = Vertices.Num() - 1; I < Vertices.Num(); J = I++)
    {
        const FVector2D& A = Vertices[I];
        const FVector2D& B = Vertices[J];
        if ((A.Y > Point.Y) == (B.Y > Point.Y))
        {
            continue;
        }
        const double XIntersect = B.X + (A.X - B.X) * (Point.Y - B.Y) / (A.Y - B.Y);
        if (Point.X < XIntersect)
        {
            bInside = !bInside;
        }
    }
    return bInside;
}
}

/** 输入已通过ValidateMask，供请求内批量EvaluatePrepared与单点ContainsPoint共享。 */
bool ContainsValidated(const FGamePlatformPCGSpatialMask& Mask, const FVector2D& Position)
{
    if (Mask.bClosed && Mask.bFillInterior && InsidePolygon(Mask.Vertices, Position))
    {
        return true;
    }
    const double WidthSquared = FMath::Square(static_cast<double>(Mask.HalfWidthCm));
    const int32 NumSegments = Mask.bClosed ? Mask.Vertices.Num() : Mask.Vertices.Num() - 1;
    for (int32 I = 0; I < NumSegments; ++I)
    {
        const FVector2D& A = Mask.Vertices[I];
        const FVector2D& B = Mask.Vertices[(I + 1) % Mask.Vertices.Num()];
        if (SegmentDistanceSquared(Position, A, B) <= FMath::Max(WidthSquared, GeometryEpsilon))
        {
            return true;
        }
    }
    return false;
}

bool FGamePlatformPCGSpatialRules::ValidateMask(const FGamePlatformPCGSpatialMask& Mask)
{
    const int32 Minimum = Mask.bClosed ? 3 : 2;
    if (!Mask.SourceId.IsValid() || Mask.Vertices.Num() < Minimum ||
        Mask.Vertices.Num() > MaxVertices || Mask.Priority < 0 || Mask.Priority > 100 ||
        !FMath::IsFinite(Mask.Strength) || Mask.Strength < 0.0f || Mask.Strength > 1.0f ||
        !FMath::IsFinite(Mask.HalfWidthCm) || Mask.HalfWidthCm < 0.0f)
    {
        return false;
    }

    for (const FVector2D& V : Mask.Vertices)
    {
        if (!FMath::IsFinite(V.X) || !FMath::IsFinite(V.Y))
        {
            return false;
        }
    }
    if (Mask.bClosed)
    {
        double TwiceArea = 0.0;
        for (int32 I = 0; I < Mask.Vertices.Num(); ++I)
        {
            const FVector2D& A = Mask.Vertices[I];
            const FVector2D& B = Mask.Vertices[(I + 1) % Mask.Vertices.Num()];
            TwiceArea += A.X * B.Y - B.X * A.Y;
        }
        if (!FMath::IsFinite(TwiceArea) || FMath::Abs(TwiceArea) <= GeometryEpsilon)
        {
            return false;
        }
    }
    return true;
}

bool FGamePlatformPCGSpatialRules::ContainsPoint(const FGamePlatformPCGSpatialMask& Mask, const FVector2D& Position)
{
    if (!ValidateMask(Mask) || !FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y))
    {
        return false;
    }
    return ContainsValidated(Mask, Position);
}

bool FGamePlatformPCGSpatialRules::Evaluate(const FVector2D& Position, int32 SubjectPriority,
    float Threshold, TConstArrayView<FGamePlatformPCGSpatialMask> Masks,
    float& OutStrength, FGuid& OutSource)
{
    OutStrength = 0.0f;
    OutSource.Invalidate();
    if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) ||
        !FMath::IsFinite(Threshold) || Threshold < 0.0f || Threshold > 1.0f ||
        Masks.Num() > 256)
    {
        return false;
    }

    TArray<FGamePlatformPCGPreparedSpatialMask> Prepared;
    return PrepareMasks(Masks, Prepared) &&
        EvaluatePrepared(Position, SubjectPriority, Threshold, Prepared, OutStrength, OutSource);
}

bool FGamePlatformPCGSpatialRules::PrepareMasks(TConstArrayView<FGamePlatformPCGSpatialMask> Masks,
    TArray<FGamePlatformPCGPreparedSpatialMask>& OutPrepared)
{
    OutPrepared.Reset();
    if (Masks.Num() > 256) { return false; }
    OutPrepared.Reserve(Masks.Num());
    for (const FGamePlatformPCGSpatialMask& Mask : Masks)
    {
        if (!ValidateMask(Mask))
        {
            OutPrepared.Reset();
            return false;
        }
        FGamePlatformPCGPreparedSpatialMask& Entry = OutPrepared.AddDefaulted_GetRef();
        Entry.Mask = &Mask;
        Entry.Min = Mask.Vertices[0];
        Entry.Max = Mask.Vertices[0];
        for (const FVector2D& P : Mask.Vertices)
        {
            Entry.Min.X = FMath::Min(Entry.Min.X, P.X);
            Entry.Min.Y = FMath::Min(Entry.Min.Y, P.Y);
            Entry.Max.X = FMath::Max(Entry.Max.X, P.X);
            Entry.Max.Y = FMath::Max(Entry.Max.Y, P.Y);
        }
        Entry.Min -= FVector2D(Mask.HalfWidthCm, Mask.HalfWidthCm);
        Entry.Max += FVector2D(Mask.HalfWidthCm, Mask.HalfWidthCm);
    }
    return true;
}

bool FGamePlatformPCGSpatialRules::EvaluatePrepared(const FVector2D& Position, int32 SubjectPriority,
    float Threshold, TConstArrayView<FGamePlatformPCGPreparedSpatialMask> Prepared,
    float& OutStrength, FGuid& OutSource)
{
    OutStrength = 0.0f;
    OutSource.Invalidate();
    if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) ||
        !FMath::IsFinite(Threshold) || Threshold < 0.0f || Threshold > 1.0f)
    {
        return false;
    }

    int32 BestPriority = MIN_int32;
    for (const FGamePlatformPCGPreparedSpatialMask& Entry : Prepared)
    {
        const FGamePlatformPCGSpatialMask* Mask = Entry.Mask;
        if (!Mask) { return false; }
        if (!FGamePlatformPCGPriorityRules::ShouldCarve(
                SubjectPriority, Mask->Priority, Mask->Strength, Threshold) ||
            Position.X < Entry.Min.X || Position.X > Entry.Max.X ||
            Position.Y < Entry.Min.Y || Position.Y > Entry.Max.Y ||
            !ContainsValidated(*Mask, Position))
        {
            continue;
        }
        const bool bHigher = Mask->Priority > BestPriority;
        const bool bTiebreak = Mask->Priority == BestPriority &&
            (!OutSource.IsValid() || Mask->SourceId.ToString(EGuidFormats::Digits) <
                OutSource.ToString(EGuidFormats::Digits));
        if (bHigher || bTiebreak)
        {
            BestPriority = Mask->Priority;
            OutStrength = Mask->Strength;
            OutSource = Mask->SourceId;
        }
    }
    return true;
}
