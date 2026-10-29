// 通用水域Editor只读审查：可从编辑器控制台执行gp.Water.Audit，输出稳定ID与区域规则问题。
// 不修改地图，不自动Spawn WaterBody，也不将流体模拟/美术数据打进Dedicated Server。
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Actors/GamePlatformWaterRegion.h"
#include "GameFramework/PhysicsVolume.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformWaterAudit, Log, All);

namespace
{
    void AuditOpenWaterWorld()
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!IsValid(World))
        {
            UE_LOG(LogGamePlatformWaterAudit, Error, TEXT("gp.Water.Audit：尚未打开有效的编辑器地图"));
            return;
        }
        int32 Count = 0;
        int32 Invalid = 0;
        TSet<FName> RegionIDs;
        int32 SwimmableRegions = 0;
        for (TActorIterator<AGamePlatformWaterRegion> It(World); It; ++It)
        {
            const AGamePlatformWaterRegion* Region = *It;
            ++Count;
            if (!IsValid(Region) || !Region->IsWaterRegionValid() ||
                RegionIDs.Contains(Region->Region.RegionId))
            {
                ++Invalid;
                UE_LOG(LogGamePlatformWaterAudit, Error,
                    TEXT("水域规则不合法或区域身份重复，Actor=%s"), IsValid(Region) ? *Region->GetName() : TEXT("None"));
                continue;
            }
            RegionIDs.Add(Region->Region.RegionId);
            if (Region->Region.bSwimmable) ++SwimmableRegions;
        }
        // 原生WaterBodyLake负责视觉和水下后处理，不负责自动建立角色游泳物理体积。
        // 审查不调用生成材质实例的getter，避免只读控制台命令修改未保存资产。
        int32 LakeCount = 0;
        int32 MissingUnderwater = 0;
        for (TActorIterator<AWaterBody> It(World); It; ++It)
        {
            const AWaterBody* Body = *It;
            if (!IsValid(Body) || Body->GetWaterBodyType() != EWaterBodyType::Lake) continue;
            ++LakeCount;
            const UWaterBodyComponent* WaterComponent = Body->GetWaterBodyComponent();
            if (!IsValid(WaterComponent) || !IsValid(WaterComponent->UnderwaterPostProcessMaterial.Get()) ||
                !WaterComponent->UnderwaterPostProcessSettings.bEnabled ||
                WaterComponent->UnderwaterPostProcessSettings.BlendWeight <= 0.f ||
                !WaterComponent->GetGenerateOverlapEvents())
            {
                ++MissingUnderwater;
                UE_LOG(LogGamePlatformWaterAudit, Warning,
                    TEXT("湖泊水下效果未完成：%s；检查后处理材质、Enabled、Overlap及BlendWeight"), *Body->GetName());
            }
        }

        int32 SwimVolumeCount = 0;
        for (TActorIterator<APhysicsVolume> It(World); It; ++It)
        {
            const APhysicsVolume* Volume = *It;
            if (IsValid(Volume) && Volume->bWaterVolume) ++SwimVolumeCount;
        }
        if (SwimmableRegions > 0 && SwimVolumeCount == 0)
        {
            UE_LOG(LogGamePlatformWaterAudit, Warning,
                TEXT("已有%d个允许游泳的权威区域，但缺少bWaterVolume=true的PhysicsVolume；角色不会自动进入MOVE_Swimming"),
                SwimmableRegions);
        }
        UE_LOG(LogGamePlatformWaterAudit, Display,
            TEXT("WaterBodyLake=%d 水下效果待完善=%d 可游泳区域=%d 游泳物理体积=%d"),
            LakeCount, MissingUnderwater, SwimmableRegions, SwimVolumeCount);
        UE_LOG(LogGamePlatformWaterAudit, Display,
            TEXT("gp.Water.Audit（地图水域审核）：地图=%s 水域数量=%d 异常=%d"),
            *World->GetName(), Count, Invalid);
    }

    FAutoConsoleCommand WaterAuditCommand(
        TEXT("gp.Water.Audit"),
        TEXT("只读审核当前编辑器地图平台水域RegionId、几何体积及规则（GamePlatformWaterEditor）"),
        FConsoleCommandDelegate::CreateStatic(&AuditOpenWaterWorld));
}

IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformWaterEditor)
