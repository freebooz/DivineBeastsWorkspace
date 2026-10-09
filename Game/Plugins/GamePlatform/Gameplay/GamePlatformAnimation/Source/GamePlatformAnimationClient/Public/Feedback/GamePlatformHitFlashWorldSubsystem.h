#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "GamePlatformHitFlashWorldSubsystem.generated.h"

class UMeshComponent;
class UMaterialInterface;

/**
 * 单个可见网格的短时闪白记录。
 * UObject强引用由UPROPERTY维护，确保原有Overlay在临时替换期间不会被GC。
 */
USTRUCT()
struct FGamePlatformHitFlashMeshRecord
{
    GENERATED_BODY()

    TWeakObjectPtr<UMeshComponent> Mesh;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> PreviousOverlay = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> ActiveOverlay = nullptr;

    double FirstStartedAtSeconds = 0.0;
    double DeadlineSeconds = 0.0;
};

/**
 * UGamePlatformHitFlashWorldSubsystem（平台客户端命中闪白服务）。
 * 复用UE5.8 UMeshComponent原生OverlayMaterial接口，不为每次命中创建MID。
 * 资源应由GamePlatformData/项目内容包事先预加载，服务不做同步资源查询。
 * 不影响服务器Gameplay、全局后处理或HUD；World作用域适配MultiPIE与分屏。
 */
UCLASS()
class GAMEPLATFORMANIMATIONCLIENT_API UGamePlatformHitFlashWorldSubsystem
    : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    /**
     * 只对受击模型显示短时Overlay，高亮不超过60Hz参考2帧。
     * 命中期间遇到重复GUID不再次闪烁；资源未加载/网格不在本World时安全拒绝。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Combat Feedback")
    bool PlayHitFlash(
        const FGuid& EventId,
        UMeshComponent* TargetMesh,
        UMaterialInterface* PreloadedFlashOverlay,
        float DurationSeconds);

    /** 地图退出、外观资产替换或人工取消时恢复本服务接管的Overlay。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Combat Feedback")
    void CancelAllHitFlashes();

    /** 调试用途，仅返回本World处于短时高亮的网格数。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Combat Feedback")
    int32 GetActiveFlashCount() const { return ActiveRecords.Num(); }

private:
    bool TickHitFlashes(float DeltaSeconds);
    void RestoreFlashAt(int32 Index);
    void RememberEvent(const FGuid& EventId);

    UPROPERTY(Transient)
    TArray<FGamePlatformHitFlashMeshRecord> ActiveRecords;

    TSet<FGuid> RecentEventIds;
    TArray<FGuid> RecentEventOrder;
    FTSTicker::FDelegateHandle ActiveTicker;

    static constexpr int32 MaxActiveFlashes = 64;
    static constexpr int32 MaxRecentEvents = 256;
    static constexpr double MaximumFlashSeconds = 2.0 / 60.0;
};
