// 单一UWorld水域索引：只按地图区域枚举，查询复杂度受128区域上限约束；
// 该索引没有时间状态，跨世界旅行和PIE销毁时清理，不可误作网络同步服务。
#include "Subsystems/GamePlatformWaterWorldSubsystem.h"
#include "Actors/GamePlatformWaterRegion.h"

#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformWater, Log, All);

bool UGamePlatformWaterWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UGamePlatformWaterWorldSubsystem::Deinitialize()
{
    Regions.Reset();
    Super::Deinitialize();
}

bool UGamePlatformWaterWorldSubsystem::RegisterRegion(AGamePlatformWaterRegion* Region)
{
    if (!IsValid(Region) || Region->GetWorld() != GetWorld() || !Region->IsWaterRegionValid())
    {
        UE_LOG(LogGamePlatformWater, Warning, TEXT("水域注册拒绝：Actor世界或规则无效"));
        return false;
    }
    // 跨地图卸载时EndPlay通常会反注册；若世界销毁顺序导致弱引用先失效，
    // 注册新区域前也要整理失效项，避免128容量门禁被已经消失的Actor占满。
    Regions.RemoveAll([](const TWeakObjectPtr<AGamePlatformWaterRegion>& Weak)
    {
        return !IsValid(Weak.Get());
    });
    for (const TWeakObjectPtr<AGamePlatformWaterRegion>& ExistingWeak : Regions)
    {
        const AGamePlatformWaterRegion* Existing = ExistingWeak.Get();
        if (Existing == Region)
            return true;
        if (IsValid(Existing) && Existing->Region.RegionId == Region->Region.RegionId)
        {
            UE_LOG(LogGamePlatformWater, Error, TEXT("重复水域ID：%s"),
                *Region->Region.RegionId.ToString());
            return false;
        }
    }
    if (Regions.Num() >= 128)
    {
        UE_LOG(LogGamePlatformWater, Error, TEXT("水域数量超出128个上限，应分区/合并或分片地图"));
        return false;
    }
    Regions.Add(Region);
    return true;
}

void UGamePlatformWaterWorldSubsystem::UnregisterRegion(const AGamePlatformWaterRegion* Region)
{
    Regions.RemoveAll([Region](const TWeakObjectPtr<AGamePlatformWaterRegion>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Region;
    });
}

bool UGamePlatformWaterWorldSubsystem::IsRegionRegistered(const AGamePlatformWaterRegion* Region) const
{
    return IsValid(Region) && Region->GetWorld() == GetWorld() && Regions.ContainsByPredicate(
        [Region](const TWeakObjectPtr<AGamePlatformWaterRegion>& Entry) { return Entry.Get() == Region; });
}

bool UGamePlatformWaterWorldSubsystem::QueryWaterAtLocation(
    const FVector& WorldLocation, FGamePlatformWaterSample& OutSample) const
{
    OutSample = FGamePlatformWaterSample();
    bool bFound = false;
    int32 SelectedPriority = -1;
    float SelectedDepth = TNumericLimits<float>::Max();
    FName SelectedRegionId = NAME_None;
    for (const TWeakObjectPtr<AGamePlatformWaterRegion>& Weak : Regions)
    {
        const AGamePlatformWaterRegion* Region = Weak.Get();
        if (!IsValid(Region))
            continue;
        FGamePlatformWaterSample Candidate;
        if (!Region->SampleRegion(WorldLocation, Candidate))
            continue;
        // 与Actor遍历/加载顺序无关：优先级→水面深度→稳定RegionId字典序。
        // 相同客户端和服务器地图Cook必须选到同一重叠区域，不依赖指针地址。
        if (!bFound || Region->Region.Priority > SelectedPriority ||
            (Region->Region.Priority == SelectedPriority &&
              (Candidate.DepthCm < SelectedDepth ||
                (Candidate.DepthCm == SelectedDepth && Candidate.RegionId.LexicalLess(SelectedRegionId)))))
        {
            bFound = true;
            SelectedPriority = Region->Region.Priority;
            SelectedDepth = Candidate.DepthCm;
            SelectedRegionId = Candidate.RegionId;
            OutSample = Candidate;
        }
    }
    return bFound;
}
