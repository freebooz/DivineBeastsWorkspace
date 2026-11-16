// 平台Surface风合同回归：端点校验、天气合并和状态去重；真实MPC/多世界在绑定测试补证。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Types/GamePlatformSurfaceTypes.h"
#include "Types/GamePlatformSurfaceParameterNames.h"
#include "Subsystems/GamePlatformSurfaceWorldSubsystem.h"
#include "Settings/GamePlatformSurfaceSettings.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSurfaceWindStateTest,
    "GamePlatform.Surface.Wind.StateValidationAndMerge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSurfaceWindStateTest::RunTest(const FString&)
{
    FGamePlatformSurfaceEnvironmentState State;
    State.Wind.To = FVector4(90.f, .25f, .2f, .1f);
    State.Wind.From = State.Wind.To;
    TestTrue(TEXT("合法风端点"), State.IsFinite());
    auto Other = State;
    Other.GlobalSnowAmount = .6f;
    TestTrue(TEXT("雨雪合并保留风"), State.Wind.IsNearlyEqual(Other.Wind));
    Other.Wind.To.Y = .7f;
    TestFalse(TEXT("风变化不能被去重"), State.IsNearlyEqual(Other));
    State.Wind.To.X = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("非法方向拒绝"), State.IsFinite());
    State.Wind.To.X = 360.f;
    TestFalse(TEXT("越界方向拒绝"), State.IsFinite());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSurfaceWindBindingTest,
    "GamePlatform.Surface.Wind.BindingAndWorldIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSurfaceWindBindingTest::RunTest(const FString&)
{
    // Transient资产只验证真实MPC实例/服务行为，不写入工程，不替代正式资产冷加载。
    TStrongObjectPtr<UWorld> WorldA(UWorld::CreateWorld(EWorldType::Game, false));
    TStrongObjectPtr<UWorld> WorldB(UWorld::CreateWorld(EWorldType::Game, false));
    TStrongObjectPtr<UMaterialParameterCollection> Collection(NewObject<UMaterialParameterCollection>());
    const FName Scalars[] = {GamePlatformSurfaceParameters::GlobalWetness, GamePlatformSurfaceParameters::GlobalSnowAmount,
        GamePlatformSurfaceParameters::GlobalSnowHeightCm, GamePlatformSurfaceParameters::GlobalMossInfluence,
        GamePlatformSurfaceParameters::GlobalPuddleAmount, GamePlatformSurfaceParameters::RainIntensity,
        GamePlatformSurfaceParameters::SnowIntensity, GamePlatformSurfaceParameters::TemperatureCelsius,
        GamePlatformSurfaceParameters::GlobalWindSeed};
    for (const FName Name : Scalars)
    {
        FCollectionScalarParameter P; P.ParameterName = Name; Collection->ScalarParameters.Add(P);
    }
    for (const FName Name : {GamePlatformSurfaceParameters::GlobalWindFrom, GamePlatformSurfaceParameters::GlobalWindTo,
        GamePlatformSurfaceParameters::GlobalWindTiming})
    {
        FCollectionVectorParameter P; P.ParameterName = Name; Collection->VectorParameters.Add(P);
    }
    TStrongObjectPtr<UGamePlatformSurfaceWorldSubsystem> A(NewObject<UGamePlatformSurfaceWorldSubsystem>(WorldA.Get()));
    TStrongObjectPtr<UGamePlatformSurfaceWorldSubsystem> B(NewObject<UGamePlatformSurfaceWorldSubsystem>(WorldB.Get()));
    TStrongObjectPtr<UMaterialParameterCollectionInstance> IA(NewObject<UMaterialParameterCollectionInstance>());
    TStrongObjectPtr<UMaterialParameterCollectionInstance> IB(NewObject<UMaterialParameterCollectionInstance>());
    IA->SetCollection(Collection.Get(), WorldA.Get()); IB->SetCollection(Collection.Get(), WorldB.Get());
    A->BoundCollection = Collection.Get(); A->BoundInstance = IA.Get(); A->bBindingAttempted = true;
    B->BoundCollection = Collection.Get(); B->BoundInstance = IB.Get(); B->bBindingAttempted = true;
    FGamePlatformSurfaceEnvironmentState StateA, StateB;
    StateA.Wind.To = StateA.Wind.From = FVector4(90, .25, .2, .1);
    StateB.Wind.To = StateB.Wind.From = FVector4(180, .5, .3, .2);
    TestEqual(TEXT("A世界完整写入12项"), A->ApplyEnvironmentState(StateA).UpdatedParameterCount, 12);
    TestEqual(TEXT("B世界完整写入"), B->ApplyEnvironmentState(StateB).Status, EGamePlatformSurfaceUpdateStatus::Applied);
    FLinearColor VA, VB;
    TestTrue(TEXT("A向量真实可读"), IA->GetVectorParameterValue(GamePlatformSurfaceParameters::GlobalWindTo, VA));
    TestTrue(TEXT("B向量真实可读"), IB->GetVectorParameterValue(GamePlatformSurfaceParameters::GlobalWindTo, VB));
    TestEqual(TEXT("A风向隔离"), VA.R, 90.f); TestEqual(TEXT("B风向隔离"), VB.R, 180.f);
    TestEqual(TEXT("同值不重复写"), A->ApplyEnvironmentState(StateA).Status, EGamePlatformSurfaceUpdateStatus::Unchanged);
    auto Merged = A->GetEnvironmentState(); Merged.GlobalSnowAmount = .6f;
    A->ApplyEnvironmentState(Merged);
    IA->GetVectorParameterValue(GamePlatformSurfaceParameters::GlobalWindTo, VA);
    TestEqual(TEXT("雨雪变化保留风端点"), VA.R, 90.f);
    // 旧八标量MPC在风升级前仍须更新雨雪；明确返回失配，并禁止半套风写入。
    TStrongObjectPtr<UMaterialParameterCollection> LegacyCollection(NewObject<UMaterialParameterCollection>());
    for (int32 Index = 0; Index < 8; ++Index)
    {
        FCollectionScalarParameter P; P.ParameterName = Scalars[Index]; LegacyCollection->ScalarParameters.Add(P);
    }
    TStrongObjectPtr<UMaterialParameterCollectionInstance> LegacyInstance(NewObject<UMaterialParameterCollectionInstance>());
    LegacyInstance->SetCollection(LegacyCollection.Get(), WorldB.Get());
    B->BoundCollection = LegacyCollection.Get(); B->BoundInstance = LegacyInstance.Get();
    TGuardValue<bool> NoExpectedWarning(B->bLoggedBindingFailure, true);
    StateB.GlobalSnowAmount = .75f;
    const auto LegacyResult = B->ApplyEnvironmentState(StateB);
    TestEqual(TEXT("旧合同必须返回失配"), LegacyResult.Status, EGamePlatformSurfaceUpdateStatus::ParameterContractMismatch);
    TestEqual(TEXT("旧八标量仍完整写入"), LegacyResult.UpdatedParameterCount, 8);
    float SnowAmount = 0.f;
    TestTrue(TEXT("旧积雪字段仍可读"), LegacyInstance->GetScalarParameterValue(GamePlatformSurfaceParameters::GlobalSnowAmount, SnowAmount));
    TestEqual(TEXT("缺风不能阻断雪"), SnowAmount, .75f);
    A->bClosing = true;
    TestEqual(TEXT("退出世界拒绝旧风"), A->ApplyEnvironmentState(StateA).Status, EGamePlatformSurfaceUpdateStatus::InvalidWorld);
    WorldA->DestroyWorld(false); WorldB->DestroyWorld(false);
    return true;
}
#endif
