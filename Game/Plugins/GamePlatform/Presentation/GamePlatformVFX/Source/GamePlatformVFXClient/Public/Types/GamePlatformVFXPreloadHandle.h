// 本文件属于GamePlatform平台层 GamePlatformVFX，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXPreloadHandle.generated.h"

class UWorld;

/** 异步预加载句柄。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXPreloadHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FGuid Id;
    /** 世界与本次预载事务代次；不与播放/目录句柄互转。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX") int64 Generation = 0;
    UPROPERTY(Transient) TWeakObjectPtr<UWorld> World;

    bool IsValid() const { return Id.IsValid() && Generation > 0 && World.IsValid(); }
};
/** 预载Ready仅在全部必需定义及本次选中资源租约成功后成立；未生成Niagara实例。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXPreloadState : uint8 { Invalid, Loading, Ready, Failed, Cancelled, WorldDestroyed };
