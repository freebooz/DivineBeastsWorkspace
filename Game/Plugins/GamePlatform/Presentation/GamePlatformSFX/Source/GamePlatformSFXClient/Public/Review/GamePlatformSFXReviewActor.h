// 平台客户端音效审核Actor：只提交既有SFX服务请求，不复制音频执行器、不保存玩法权威状态。
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/GamePlatformSFXTypes.h"
#include "GamePlatformSFXReviewActor.generated.h"

class USceneComponent;

/** 可在开发审核地图中调整定义/位置并试听；不复制、无Tick、Shipping禁用播放。 */
UCLASS(BlueprintType, Blueprintable)
class GAMEPLATFORMSFXCLIENT_API AGamePlatformSFXReviewActor : public AActor
{
    GENERATED_BODY()
public:
    AGamePlatformSFXReviewActor();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    /** 稳定逻辑定义，例如review.sfx.twod@1；不是资产路径，声音仍由Data租约加载。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Review|SFX")
    FName DefinitionId = NAME_None;
    /** 默认不自动发声；需在开发地图实例明确启用，纯编辑器打开地图永不发声。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Review|SFX")
    bool bPlayOnBeginPlay = false;
    /** BeginPlay后延迟秒数0..60，用于不同播放站错开试听；EndPlay清理定时器。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Review|SFX", meta=(ClampMin="0", ClampMax="60"))
    float StartDelaySeconds = 0.1f;
    /** 请求音量0..1，默认0.35降低审核场景的意外高声压；与Definition音量相乘。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Review|SFX", meta=(ClampMin="0", ClampMax="1"))
    float VolumeMultiplier = 0.35f;
    /** 服务最后一次接纳/拒绝码；Queued不代表听觉验收成功。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Review|SFX")
    EGamePlatformSFXResultCode LastResult = EGamePlatformSFXResultCode::InvalidRequest;

    /** 当前World必须是客户端Game/PIE/GamePreview而非专服/销毁中；不创建任何新服务。 */
    static bool IsSupportedReviewWorld(const UWorld* World);
    /** 游戏线程开始/替换自己的一次审核请求；无效定义不返回固定成功。 */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="GamePlatform|Review|SFX")
    bool PlayReview();
    /** 游戏线程取消自己的延迟播放并停止自有RequestId；可重复调用，先清成员再通知服务。 */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="GamePlatform|Review|SFX")
    void StopReview();
    /** 读取当前自有句柄是否仍在加载/播放，不轮询或扫描世界对象。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Review|SFX")
    bool IsReviewActive() const;
    UFUNCTION(BlueprintPure, Category="GamePlatform|Review|SFX")
    EGamePlatformSFXPlaybackState GetReviewPlaybackState() const;
    /** 世界服务诊断用于自动审核，值不是音质/性能合格证书。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Review|SFX")
    FGamePlatformSFXDiagnostics GetReviewDiagnostics() const;
    UFUNCTION(BlueprintPure, Category="GamePlatform|Review|SFX")
    FGuid GetActiveRequestId() const { return ActiveRequestId; }
private:
#if WITH_DEV_AUTOMATION_TESTS
    // 仅原生自动化可观察自有延迟定时器；不向运行时调用方公开内部TimerHandle。
    friend class FGamePlatformSFXReviewActorTest;
#endif
    UPROPERTY(VisibleAnywhere, Category="Review|SFX")
    TObjectPtr<USceneComponent> ReviewRoot;
    FGamePlatformSFXHandle ActiveHandle;
    FGuid ActiveRequestId;
    FTimerHandle StartTimer;
    uint64 RequestEpoch = 0;
    bool bClosing = false;
};
