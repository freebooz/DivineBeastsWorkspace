#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GamePlatformLoadingScreenService.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformLoadingToken
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    FGuid Id;

    bool IsValid() const { return Id.IsValid(); }
};

/** 单个加载事务可提供的可选画面提示。多个加载事务并发时保守保持遮罩。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUILoadingPresentation
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    bool bShowWorld = false;

    /** 当前世界的服务端秒钟。0 表示无倒计时，归零仅改变显示。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    double CountdownDeadlineServerTime = 0.0;

    bool IsEquivalent(const FGamePlatformUILoadingPresentation& Other) const
    {
        return bShowWorld == Other.bShowWorld &&
            CountdownDeadlineServerTime == Other.CountdownDeadlineServerTime;
    }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUILoadingSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    bool bIsLoading = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    int32 ActiveTokenCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    FText Stage;

    /** 小于0表示权威进度未知，UI不得伪造0~100。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    float Progress = -1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Loading")
    FGamePlatformUILoadingPresentation Presentation;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformUILoadingSnapshotChanged,
    const FGamePlatformUILoadingSnapshot&, Snapshot);

/**
 * UGamePlatformLoadingScreenService（游戏平台加载界面令牌聚合服务）。
 *
 * 通过 Token（令牌）聚合多个并发加载事务，向界面广播真实加载快照。
 * 性能上使用缓存快照：状态变化时重建一次，GetSnapshot 为 O(1) 读取。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformLoadingScreenService : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="UI|Loading")
    FGamePlatformLoadingToken AcquireToken(FText Stage, float Progress = -1.0f);

    UFUNCTION(BlueprintCallable, Category="UI|Loading")
    bool UpdateToken(FGamePlatformLoadingToken Token, FText Stage, float Progress = -1.0f);

    /** 与既有蓝图接口共用同一令牌聚合，供游戏线程上的项目加载适配器提交只读画面提示。 */
    FGamePlatformLoadingToken AcquirePresentationToken(FText Stage, float Progress,
        FGamePlatformUILoadingPresentation Presentation);
    bool UpdatePresentationToken(FGamePlatformLoadingToken Token, FText Stage, float Progress,
        FGamePlatformUILoadingPresentation Presentation);

    UFUNCTION(BlueprintCallable, Category="UI|Loading")
    bool ReleaseToken(FGamePlatformLoadingToken Token);

    UFUNCTION(BlueprintCallable, Category="UI|Loading")
    void ReleaseAll();

    UFUNCTION(BlueprintPure, Category="UI|Loading")
    FGamePlatformUILoadingSnapshot GetSnapshot() const;

    UPROPERTY(BlueprintAssignable, Category="UI|Loading")
    FGamePlatformUILoadingSnapshotChanged OnSnapshotChanged;

private:
    struct FTokenState
    {
        FText Stage;
        float Progress = -1.0f;
        FGamePlatformUILoadingPresentation Presentation;
        uint64 Sequence = 0;
    };

    TMap<FGuid, FTokenState> Tokens;
    uint64 NextSequence = 1;

    /** 最近一次重建的快照；避免每个 UI 读取者重复遍历 Token。 */
    UPROPERTY(Transient)
    FGamePlatformUILoadingSnapshot CachedSnapshot;

    /** 在 Token 集合变化后重建一次缓存快照。 */
    void RebuildSnapshot();

    void BroadcastSnapshot();
};
