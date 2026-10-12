// 项目鸟群演示操作：全部复用正式SceneComponent/Presentation/Data/VFX，不触发旧ISM渲染器。
// 所有数字必须区分请求数量与GPU真实存活粒子；未经Niagara读回时后者为INDEX_NONE。
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "DivineBeastsPresentationClientSubsystem.h"
#include "Environment/DivineBeastsBirdFlockSceneComponent.h"
#include "Environment/DivineBeastsBirdFlockBatchComponent.h"
#include "Environment/BirdFlock/GamePlatformBirdFlockSplineMath.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"

namespace
{
    /** 按World筛选真实已注册的项目鸟群组件；不同PIE/已退出关卡对象不可参与本演示。 */
    template <typename Visitor>
    void ForEachFormalBird(UWorld* World, Visitor&& Apply)
    {
        if (!World || World->bIsTearingDown) return;
        for (TObjectIterator<UDivineBeastsBirdFlockSceneComponent> It; It; ++It)
        {
            auto* Bird = *It;
            if (IsValid(Bird) && Bird->GetWorld() == World && Bird->IsRegistered() &&
                IsValid(Bird->GetOwner()) && !Bird->GetOwner()->IsActorBeingDestroyed())
            {
                Apply(*Bird);
            }
        }
    }

    /** 分批群组为受控组件，不是一鸟一Actor。 */
    template <typename Visitor>
    void ForEachFormalBatch(UWorld* World, Visitor&& Apply)
    {
        if (!World || World->bIsTearingDown) return;
        for (TObjectIterator<UDivineBeastsBirdFlockBatchComponent> It; It; ++It)
        {
            auto* Batch = *It;
            if (IsValid(Batch) && Batch->GetWorld() == World && Batch->IsRegistered() &&
                IsValid(Batch->GetOwner())) Apply(*Batch);
        }
    }
}

int32 UDivineBeastsDemoWorldSubsystem::GetBirdActorCount() const
{
    int32 Count = 0;
    ForEachFormalBird(GetWorld(), [&Count](const UDivineBeastsBirdFlockSceneComponent&) { ++Count; });
    return Count;
}

int32 UDivineBeastsDemoWorldSubsystem::GetActualBirdCount() const
{
    // UI不能使用旧ISM实例数量或RequestedCount冒充当前GPU粒子；无实读回就明确返回-1。
    return INDEX_NONE;
}

int32 UDivineBeastsDemoWorldSubsystem::GetRequestedBirdCount() const
{
    UWorld* World = GetWorld();
    UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    ULocalPlayer* Player = Instance ? Instance->GetFirstGamePlayer() : nullptr;
    const auto* Service = Player ? Player->GetSubsystem<UDivineBeastsPresentationClientSubsystem>() : nullptr;
    if (!Service) return 0;
    int32 Requested = 0;
    ForEachFormalBird(World, [&](const UDivineBeastsBirdFlockSceneComponent& Bird)
    {
        Requested += Service->GetRequestedBirdCount(Bird.GetConfiguredFlockId());
    });
    return Requested;
}

void UDivineBeastsDemoWorldSubsystem::CaptureOriginalBird(
    UDivineBeastsBirdFlockSceneComponent& Bird)
{
    if (OriginalBirds.Contains(&Bird)) return;
    FOriginalBirdState Original;
    Original.Count = Bird.BirdCount;
    Original.Species = Bird.SpeciesId;
    Original.bAdvanced = Bird.bEnableAdvancedFlight;
    Original.Flight = Bird.AdvancedFlight;
    OriginalBirds.Add(&Bird, MoveTemp(Original));
}

bool UDivineBeastsDemoWorldSubsystem::CommitBirdChange(
    UDivineBeastsBirdFlockSceneComponent& Bird,
    const FOriginalBirdState& Before, FString& OutError)
{
    if (Bird.ApplyBirdFlock(OutError)) return true;
    // 旧Niagara请求因正式组件事务尚未取消；同时恢复界面读到的本地字段。
    Bird.BirdCount = Before.Count;
    Bird.SpeciesId = Before.Species;
    Bird.bEnableAdvancedFlight = Before.bAdvanced;
    Bird.AdvancedFlight = Before.Flight;
    return false;
}

bool UDivineBeastsDemoWorldSubsystem::ApplyBirdEdits(
    const TCHAR* Description, TFunctionRef<void(UDivineBeastsBirdFlockSceneComponent&)> Edit)
{
    if (!FDivineBeastsPluginDemoMode::IsEnabled())
    {
        LastFeedback = FText::FromString(TEXT("只有明确启动的插件演示模式可以修改鸟群。"));
        return false;
    }
    int32 Accepted = 0;
    int32 Failed = 0;
    FString LatestError;
    ForEachFormalBird(GetWorld(), [&](UDivineBeastsBirdFlockSceneComponent& Bird)
    {
        FOriginalBirdState Before;
        Before.Count = Bird.BirdCount;
        Before.Species = Bird.SpeciesId;
        Before.bAdvanced = Bird.bEnableAdvancedFlight;
        Before.Flight = Bird.AdvancedFlight;
        CaptureOriginalBird(Bird);
        Edit(Bird);
        FString Error;
        if (CommitBirdChange(Bird,Before,Error)) ++Accepted;
        else { ++Failed; LatestError = Error; }
    });
    LastFeedback = FText::FromString(FString::Printf(
        TEXT("%s：正式表现请求受理%d，失败%d。%s GPU粒子仍需真实读回。"),
        Description,Accepted,Failed,*LatestError));
    return Accepted > 0 && Failed == 0;
}

bool UDivineBeastsDemoWorldSubsystem::ApplyBirdCount(int32 Count)
{
    if (Count < 0 || Count > 512)
    {
        LastFeedback=FText::FromString(TEXT("鸟群数量必须在0至512。"));
        return false;
    }
    return ApplyBirdEdits(TEXT("鸟群数量"),[Count](UDivineBeastsBirdFlockSceneComponent& Bird)
    {
        Bird.BirdCount = Count;
    });
}

bool UDivineBeastsDemoWorldSubsystem::ApplyBirdQuality(float Fraction)
{
    if (!FMath::IsFinite(Fraction) || Fraction < 0.f || Fraction > 1.f)
    {
        LastFeedback=FText::FromString(TEXT("画质比例必须在0至1。"));
        return false;
    }
    return ApplyBirdEdits(TEXT("鸟群质量比例"),[Fraction](UDivineBeastsBirdFlockSceneComponent& Bird)
    {
        Bird.bEnableAdvancedFlight=true;
        Bird.AdvancedFlight.QualityFraction=Fraction;
    });
}

bool UDivineBeastsDemoWorldSubsystem::ApplyBirdSpecies(FName SpeciesId)
{
    if (SpeciesId != TEXT("Seagull") && SpeciesId != TEXT("Swallow") && SpeciesId != TEXT("Eagle"))
    {
        LastFeedback=FText::FromString(TEXT("物种未登记：只支持项目正式海鸥、燕子、雄鹰目录。"));
        return false;
    }
    return ApplyBirdEdits(TEXT("鸟群物种"),[SpeciesId](UDivineBeastsBirdFlockSceneComponent& Bird)
    {
        Bird.SpeciesId=SpeciesId;
    });
}

bool UDivineBeastsDemoWorldSubsystem::ApplyBirdFlightMode(EDivineBeastsBirdFlockFlightMode Mode)
{
    if (static_cast<uint8>(Mode)>5)
    {
        LastFeedback=FText::FromString(TEXT("飞行模式不合法。"));
        return false;
    }
    return ApplyBirdEdits(TEXT("路径模式"),[Mode](UDivineBeastsBirdFlockSceneComponent& Bird)
    {
        Bird.bEnableAdvancedFlight=true;
        Bird.AdvancedFlight.FlightMode=Mode;
        Bird.AdvancedFlight.bUseSpline=static_cast<uint8>(Mode)>=3;
    });
}

bool UDivineBeastsDemoWorldSubsystem::ApplyBirdFlocking(
    float SeparationWeight,float AlignmentWeight,float CohesionWeight)
{
    if (!FGamePlatformBirdFlockSplineMath::ValidateFlockingWeights(
        SeparationWeight,AlignmentWeight,CohesionWeight))
    {
        LastFeedback=FText::FromString(TEXT("Boids权重需非负且三项之和不超过4。"));
        return false;
    }
    return ApplyBirdEdits(TEXT("群聚行为"),[=](UDivineBeastsBirdFlockSceneComponent& Bird)
    {
        Bird.bEnableAdvancedFlight=true;
        Bird.AdvancedFlight.bEnableFlocking=true;
        Bird.AdvancedFlight.SeparationWeight=SeparationWeight;
        Bird.AdvancedFlight.AlignmentWeight=AlignmentWeight;
        Bird.AdvancedFlight.CohesionWeight=CohesionWeight;
    });
}

bool UDivineBeastsDemoWorldSubsystem::ApplyBirdGlide(float GlideBlend)
{
    if (!FMath::IsFinite(GlideBlend) || GlideBlend<0.f || GlideBlend>1.f)
    {
        LastFeedback=FText::FromString(TEXT("滑翔混合比例需在0至1。"));
        return false;
    }
    return ApplyBirdEdits(TEXT("滑翔混合"),[GlideBlend](UDivineBeastsBirdFlockSceneComponent& Bird)
    {
        Bird.bEnableAdvancedFlight=true;
        Bird.AdvancedFlight.GlideBlend=GlideBlend;
    });
}

bool UDivineBeastsDemoWorldSubsystem::StartBirdBatches()
{
    if (!FDivineBeastsPluginDemoMode::IsEnabled()) return false;
    int32 Accepted=0,Failed=0;
    FString Error;
    ForEachFormalBatch(GetWorld(), [&](UDivineBeastsBirdFlockBatchComponent& Batch)
    {
        FString Failure;
        if (Batch.StartBatches(Failure)) ++Accepted;
        else { ++Failed; Error=Failure; }
    });
    LastFeedback=FText::FromString(FString::Printf(
        TEXT("正式鸟群分批生成：启动%d，失败%d。%s"),Accepted,Failed,*Error));
    return Accepted>0 && Failed==0;
}

void UDivineBeastsDemoWorldSubsystem::StopBirdBatches()
{
    if (!FDivineBeastsPluginDemoMode::IsEnabled()) return;
    ForEachFormalBatch(GetWorld(), [](UDivineBeastsBirdFlockBatchComponent& Batch)
    {
        Batch.StopBatches();
    });
}
