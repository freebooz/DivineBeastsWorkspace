// 原生几何回归：200总量、24米间距、22米保护及平台空间掩码；不冒充地图验收。
#include "Village/DivineBeastsVillageWolfSpawnPolicy.h"
#include "Services/GamePlatformPCGSpatialRules.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageWolfSpawnPolicyTest,
    "DivineBeasts.Server.VillageWolf.SpawnPolicyTwoHundred",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsVillageWolfSpawnPolicyTest::RunTest(const FString&)
{
    using P=FDivineBeastsVillageWolfSpawnPolicy;
    TestEqual(TEXT("两类怪物合计上限200"),P::MaxAliveWolves,200);
    TestEqual(TEXT("每批仍为4只"),P::MaxWolvesPerBatch,4);
    TestTrue(TEXT("平地保留原4cm净距"),FMath::IsNearlyEqual(P::SpawnHeightOnSlope(85.f,55.f,1.f),89.f));
    const float SlopeHeight=P::SpawnHeightOnSlope(85.f,55.f,.75f);
    TestTrue(TEXT("斜面胶囊球端仍须保留法线方向4cm净距"),
        FMath::IsNearlyEqual((SlopeHeight-(85.f-55.f))*.75f-55.f,4.f));
    TestFalse(TEXT("恰好24米拒绝"),P::HasSpawnSeparation(FVector::ZeroVector,FVector(2400,0,0)));
    TestTrue(TEXT("超过24米通过"),P::HasSpawnSeparation(FVector::ZeroVector,FVector(2401,0,0)));
    TestFalse(TEXT("高差不能绕过水平间距"),P::HasSpawnSeparation(FVector::ZeroVector,FVector(100,0,10000)));
    TestFalse(TEXT("出生22米内拒绝"),P::IsOutsidePlayerStart(FVector(2199,0,0),FVector::ZeroVector));
    TestTrue(TEXT("出生22米外通过其它检查"),P::IsOutsidePlayerStart(FVector(2201,0,0),FVector::ZeroVector));
    TArray<FVector> Anchors;
    for(int32 Y=0;Y<39;++Y) for(int32 X=0;X<39;++X)
    {
        const FVector Point(-24500+X*P::CandidateGridStepCm,-24500+Y*P::CandidateGridStepCm,0);
        bool Safe=P::IsOutsidePlayerStart(Point,FVector::ZeroVector);
        for(const FVector& Existing:Anchors) if(!P::HasSpawnSeparation(Point,Existing)){Safe=false;break;}
        if(Safe) Anchors.Add(Point);
    }
    TestTrue(TEXT("平地网格容量足以容纳200只及备用点"),Anchors.Num()>=P::MaxAliveWolves);
    FGamePlatformPCGSpatialMask Mask;
    Mask.SourceId=FGuid::NewGuid();Mask.DomainId=TEXT("Water.Lake");Mask.Priority=100;
    Mask.bClosed=true;Mask.Vertices={FVector2D(-1000,-1000),FVector2D(1000,-1000),FVector2D(1000,1000),FVector2D(-1000,1000)};
    TArray<FGamePlatformPCGSpatialMask> Masks={Mask};
    TArray<FGamePlatformPCGPreparedSpatialMask> Prepared;
    TestTrue(TEXT("复用平台预计算空间掩码"),FGamePlatformPCGSpatialRules::PrepareMasks(Masks,Prepared));
    float Strength=0;FGuid Source;
    TestTrue(TEXT("湖内查询合法"),FGamePlatformPCGSpatialRules::EvaluatePrepared(FVector2D::ZeroVector,0,.5f,Prepared,Strength,Source));
    TestTrue(TEXT("湖内必须排除"),Strength>=.5f);
    TestTrue(TEXT("湖外查询合法"),FGamePlatformPCGSpatialRules::EvaluatePrepared(FVector2D(2000,0),0,.5f,Prepared,Strength,Source));
    TestEqual(TEXT("湖外不应误排除"),Strength,0.f);
    return true;
}
#endif
