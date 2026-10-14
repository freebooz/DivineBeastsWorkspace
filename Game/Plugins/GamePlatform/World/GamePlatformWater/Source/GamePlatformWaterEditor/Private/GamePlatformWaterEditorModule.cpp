// 通用水域Editor只读审查：可从编辑器控制台执行gp.Water.Audit，输出稳定ID与区域规则问题。
// 不修改地图，不自动Spawn WaterBody，也不将流体模拟/美术数据打进Dedicated Server。
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"
#include "Actors/GamePlatformWaterRegion.h"

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
        }
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
