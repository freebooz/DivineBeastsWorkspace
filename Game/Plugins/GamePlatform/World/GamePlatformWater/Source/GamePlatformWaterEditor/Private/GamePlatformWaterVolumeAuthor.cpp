// 唯一编辑器几何作者：从渲染湖岸烘焙服务器也能使用的原生物理水域。
#include "GamePlatformWaterEditorLibrary.h"
#include "GamePlatformWaterBrushBuilder.h"
#include "Actors/GamePlatformWaterRegion.h"
#include "ActorFactories/ActorFactory.h"
#include "WaterBodyLakeActor.h"
#include "WaterBodyComponent.h"
#include "WaterSplineComponent.h"
#include "WaterRuntimeSettings.h"
#include "GameFramework/PhysicsVolume.h"
#include "Components/BrushComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Materials/MaterialInterface.h"
#include "CompGeom/PolygonTriangulation.h"
#include "Algo/Reverse.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Brush.h"
#include "Engine/Polys.h"
#include "Model.h"

bool UGamePlatformWaterBrushBuilder::Build(UWorld* World, ABrush* Brush)
{
    if (!Brush || !Brush->Brush || !Brush->Brush->Polys ||
        Shore.Num() < 3 || !FMath::IsFinite(DepthCm) || DepthCm <= 0.f) return false;
    TArray<UE::Geometry::FIndex3i> Triangles;
    PolygonTriangulation::TriangulateSimplePolygon(Shore, Triangles, false);
    if (Triangles.Num() != Shore.Num() - 2) return false;
    // UEditorBrushBuilder的多项虚函数未导出；直接填写原生Factory已创建的Model/Polys，
    // 后续BSP分解及BodySetup碰撞仍完全由UActorFactory::CreateBrushForVolumeActor完成。
    auto& Polygons = Brush->Brush->Polys->Element;
    Polygons.Reset();
    const auto AddFace = [&](std::initializer_list<FVector3f> Face)
    {
        FPoly P; P.Init();
        for (const auto V : Face) P.Vertices.Add(V);
        P.Base = P.Vertices[0];
        if (P.Finalize(Brush, 1) != 0) return false;
        Polygons.Add(MoveTemp(P));
        return true;
    };
    const auto At = [&](int32 I, float Z) { return FVector3f(Shore[I].X,Shore[I].Y,Z); };
    const int32 N = Shore.Num();
    for (const auto T : Triangles)
    {
        if (!AddFace({At(T.A,0),At(T.B,0),At(T.C,0)}) ||
            !AddFace({At(T.C,-DepthCm),At(T.B,-DepthCm),At(T.A,-DepthCm)})) return false;
    }
    for (int32 I = 0; I < N; ++I)
        if (!AddFace({At(I,-DepthCm),At((I+1)%N,-DepthCm),At((I+1)%N,0),At(I,0)})) return false;
    return true;
}

APhysicsVolume* UGamePlatformWaterEditorLibrary::BakeLakePhysicsVolume(
    AActor* Source, AGamePlatformWaterRegion* Region, float DepthCm, FString& OutError)
{
    OutError.Reset();
    const auto* Lake = Cast<AWaterBodyLake>(Source);
    UWorld* World = IsValid(Lake) ? Lake->GetWorld() : nullptr;
    const auto Fail = [&OutError](const TCHAR* Message) -> APhysicsVolume* { OutError = Message; return nullptr; };
    if (!IsInGameThread() || !GEditor || !World || World->WorldType != EWorldType::Editor ||
        !IsValid(Region) || Region->GetWorld() != World || !Region->Region.IsValid() || Region->WaterPhysicsVolume ||
        !FMath::IsFinite(DepthCm) || DepthCm < 100.f || DepthCm > 20000.f)
        return Fail(TEXT("要求同一Editor世界、有效Region、空体积绑定及100~20000cm深度。"));
    UWaterBodyComponent* Water = Lake->GetWaterBodyComponent();
    UWaterSplineComponent* Spline = Water ? Water->GetWaterSpline() : nullptr;
    if (!Spline || !Spline->IsClosedLoop() || Water->bAffectsLandscape || Spline->GetNumberOfSplinePoints() < 3)
        return Fail(TEXT("仅支持禁雕刻且闭合的原生湖岸。"));
    const FVector Origin = Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
    TArray<FVector2D> Points;
    bool bInvalid = false;
    const auto At = [&](float Key) { return Spline->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World); };
    TFunction<void(float,float,int32)> Flatten;
    Flatten = [&](float A, float B, int32 Depth)
    {
        if (bInvalid) return;
        const FVector P = At(A), Q = At(B);
        double Error = 0.;
        for (float Fraction : {0.25f,0.5f,0.75f})
            Error = FMath::Max(Error, FMath::PointDistToSegment(At(FMath::Lerp(A,B,Fraction)), P, Q));
        if (Error > 2. && Depth < 12) { Flatten(A,(A+B)*.5f,Depth+1); Flatten((A+B)*.5f,B,Depth+1); return; }
        if (Error > 2. || Points.Num() >= 2048 || P.ContainsNaN() || FMath::Abs(P.Z-Origin.Z) > .1) { bInvalid = true; return; }
        Points.Add(FVector2D(P.X-Origin.X,P.Y-Origin.Y));
    };
    for (int32 I = 0; I < Spline->GetNumberOfSplinePoints(); ++I) Flatten(I,I+1,0);
    if (bInvalid || Points.Num() < 3) return Fail(TEXT("湖岸不平、非有限、超过2048点或曲线逼近误差超过2cm。"));
    double Area = 0;
    for (int32 I = 0; I < Points.Num(); ++I) Area += FVector2D::CrossProduct(Points[I], Points[(I+1)%Points.Num()]);
    if (FMath::Abs(Area) < 1.) return Fail(TEXT("湖岸面积无效。"));
    if (Area < 0) Algo::Reverse(Points);
    // 禁止自交或非邻边接触；作者失败不能靠三角化静默填满凹岸。
    const auto Cross = [](FVector2D A,FVector2D B,FVector2D C) { return FVector2D::CrossProduct(B-A,C-A); };
    const auto OnSegment = [&Cross](FVector2D A,FVector2D B,FVector2D P)
    {
        return FMath::Abs(Cross(A,B,P)) < 1.e-4 &&
            P.X >= FMath::Min(A.X,B.X) && P.X <= FMath::Max(A.X,B.X) &&
            P.Y >= FMath::Min(A.Y,B.Y) && P.Y <= FMath::Max(A.Y,B.Y);
    };
    for (int32 I=0; I<Points.Num(); ++I) for (int32 J=I+2; J<Points.Num(); ++J)
    {
        if ((J+1)%Points.Num()==I) continue;
        const auto A=Points[I],B=Points[(I+1)%Points.Num()],C=Points[J],D=Points[(J+1)%Points.Num()];
        if ((Cross(A,B,C)*Cross(A,B,D) < 0 && Cross(C,D,A)*Cross(C,D,B) < 0) ||
            OnSegment(A,B,C) || OnSegment(A,B,D) || OnSegment(C,D,A) || OnSegment(C,D,B))
            return Fail(TEXT("湖岸自交，拒绝烘焙物理水域。"));
    }
    UGamePlatformWaterBrushBuilder* Builder = NewObject<UGamePlatformWaterBrushBuilder>();
    Builder->Shore = MoveTemp(Points); Builder->DepthCm = DepthCm;
    FActorSpawnParameters Spawn; Spawn.OverrideLevel = Region->GetLevel(); Spawn.ObjectFlags |= RF_Transactional;
    APhysicsVolume* Volume = World->SpawnActor<APhysicsVolume>(APhysicsVolume::StaticClass(), FTransform(Origin), Spawn);
    if (!Volume) return Fail(TEXT("物理体积创建失败。"));
    Volume->bWaterVolume = Region->Region.bSwimmable;
    Volume->bPhysicsOnContact = false;
    Volume->Priority = Region->Region.Priority + 1;
    Volume->FluidFriction = .3f;
    Volume->SetReplicates(false);
    UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
    Volume->SetActorLocation(Origin);
    Volume->GetBrushComponent()->SetCanEverAffectNavigation(false);
    Volume->GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Volume->SetActorLabel(Region->Region.RegionId.ToString() + TEXT("_PhysicsVolume"));
    const UBodySetup* Body = Volume->GetBrushComponent()->GetBodySetup();
    if (!IsValid(Body) || Body->AggGeom.GetElementCount() == 0)
    {
        Volume->Destroy();
        return Fail(TEXT("Brush没有有效烘焙碰撞，拒绝发布。"));
    }
    Region->Modify(); Region->SetActorLocation(Origin); Region->Region.SurfaceOffsetCm = 0;
    Region->WaterPhysicsVolume = Volume; Region->bRequirePhysicsVolume = true;
    Region->MarkPackageDirty();
    return Volume;
}

bool UGamePlatformWaterEditorLibrary::ConfigureLakeUnderwater(AActor* Source, UMaterialInterface* Material, FString& OutError)
{
    OutError.Reset();
    AWaterBodyLake* Lake = Cast<AWaterBodyLake>(Source);
    auto* Water = IsValid(Lake) ? Lake->GetWaterBodyComponent() : nullptr;
    if (!IsInGameThread() || !GEditor || !Water || Lake->GetWorld()->WorldType != EWorldType::Editor ||
        Water->bAffectsLandscape || !IsValid(Material))
    { OutError = TEXT("要求禁雕刻Editor湖体和有效水下材质。"); return false; }
    const ECollisionChannel Channel = GetDefault<UWaterRuntimeSettings>()->CollisionChannelForWaterTraces;
    if (Channel == ECC_Pawn || Channel == ECC_Camera)
    { OutError = TEXT("Water查询通道不能复用Pawn或Camera阻挡通道。"); return false; }
    Water->Modify();
    Water->SetCanEverAffectNavigation(false);
    Water->SetCollisionResponseToAllChannels(ECR_Ignore);
    Water->SetCollisionResponseToChannel(Channel,ECR_Block);
    Water->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Water->SetUnderwaterPostProcessMaterial(Material);
    Water->PostEditChange();
    Lake->MarkPackageDirty();
    if (Water->GetCollisionComponents(true).Num() == 0)
    { OutError = TEXT("未生成真实湖体查询碰撞，禁止保存。"); return false; }
    return true;
}
