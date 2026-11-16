#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "TimerManager.h" // 退出混合完成前延迟卸载展示舞台。
#include "Water/DivineBeastsCharacterStudioWaterPolicy.h"
#include "DivineBeastsCharacterPreviewSubsystem.generated.h"

class AGamePlatformCharacterPreviewStage;
class ADefaultPawn;
class UDivineBeastsCharacterAppearanceProfile;
class ULevelStreamingDynamic;
class UMaterialInstanceDynamic;
class UGamePlatformCinematicCameraProfile;
struct FStreamableHandle;

/**
 * UDivineBeastsCharacterPreviewSubsystem（神兽联盟角色三维预览子系统）。
 *
 * 只存在于客户端表现层，用于登录后的 CharacterEntry/CreateCharacter 三维预览：
 * - 动态流送 DBAFrontEndPack 中的 L_DBA_CharacterStudio；
 * - 复用现有 DA_Appearance_Zodiac_*，不创建第二套角色外观数据；
 * - 不生成权威 ACharacter，不接入 GAS/Combat/Inventory/AI，也不复制网络状态；
 * - 旧异步请求通过 RequestGeneration 丢弃，切换英雄不会被迟到资源覆盖。
 */
UCLASS()
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsCharacterPreviewSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 确保角色预览工作室已流送；重复调用幂等。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|CharacterPreview")
    bool ActivatePreviewScene();

    /** 卸载角色预览工作室并恢复进入预览前的 ViewTarget。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|CharacterPreview")
    void DeactivatePreviewScene();

    /** 异步预览一个稳定 HeroDefinitionId；返回是否接受请求。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|CharacterPreview")
    bool PreviewHero(FName HeroDefinitionId);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|CharacterPreview")
    void ClearPreview();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|CharacterPreview")
    void RotatePreview(float DeltaYawDegrees);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|CharacterPreview")
    void SetPreviewCameraDistance(float DistanceCentimeters);

    /** 按轮输入缩放本地预览距离；无有效舞台时返回false。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|CharacterPreview")
    bool AdjustPreviewCameraDistance(float DeltaCentimeters);

    /** 设置等效镜头焦距（毫米），限制在24至85毫米的正式展示范围。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|CharacterPreview")
    bool SetPreviewCameraFocalLength(float FocalLengthMm);

    /** 按轮输入调整等效镜头焦距（毫米）。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|CharacterPreview")
    bool AdjustPreviewCameraFocalLength(float DeltaFocalLengthMm);

    UFUNCTION(BlueprintPure, Category="DivineBeasts|CharacterPreview")
    float GetPreviewCameraDistance() const;

    UFUNCTION(BlueprintPure, Category="DivineBeasts|CharacterPreview")
    float GetPreviewCameraFocalLength() const;

    /** 场景风格切换仅替换VFX Definition，不重新流送工作室或改变角色。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|CharacterPreview")
    void SetPreviewFoliageStyle(FName StyleId);

    /** 返回当前已流送角色工作室的水效合资格状态；默认Native。
     * FluidFluxEligible只代表可选插件可用，不代表已经启动GPU水面求解。
     */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|CharacterPreview|Water")
    EDivineBeastsCharacterStudioWaterMode GetCharacterStudioWaterMode(
        bool bPreferFluidFlux, bool bHighQuality) const;

    UFUNCTION(BlueprintPure, Category="DivineBeasts|CharacterPreview")
    FName GetPreviewHeroDefinitionId() const { return RequestedHeroDefinitionId; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|CharacterPreview")
    bool IsPreviewSceneReady() const { return PreviewStage.IsValid(); }

private:
    UFUNCTION()
    void HandlePreviewLevelShown();

    void HandleWorldCleanup(
        UWorld* World,
        bool bSessionEnded,
        bool bCleanupResources);

    /** 角色选择/创建复用一组本地世界环境粒子，世界和子关卡变化时按代次重建。 */
    void EnsurePreviewFoliage();
    void ResolvePreviewStage();
    void EnsurePreviewCameraProfile();
    bool ApplyPreviewCameraProfile();
    void BindPreviewCamera();
    void DeactivatePreviewSceneInternal(bool bAllowCameraBlend);
    void CompletePreviewSceneDeactivation();
    void CancelPendingLoads();
    void HandleProfileLoaded(
        UDivineBeastsCharacterAppearanceProfile* Profile,
        FName ExpectedHeroDefinitionId,
        int32 ExpectedRequestGeneration);
    void HandleVisualResourcesLoaded(
        UDivineBeastsCharacterAppearanceProfile* Profile,
        FName ExpectedHeroDefinitionId,
        int32 ExpectedRequestGeneration);
    bool TryApplyPendingAppearance();

    UPROPERTY(Transient)
    TObjectPtr<ULevelStreamingDynamic> PreviewStreamingLevel = nullptr;

    UPROPERTY(Transient)
    TWeakObjectPtr<AGamePlatformCharacterPreviewStage> PreviewStage;

    /** 仅借用当前本地默认观测Pawn的可见性，避免球体遮挡脚部；退出时恢复原值。 */
    UPROPERTY(Transient)
    TWeakObjectPtr<ADefaultPawn> HiddenPreviewObserver;
    bool bObserverWasHidden = false;

    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsCharacterAppearanceProfile> PendingProfile = nullptr;

    /** 通过平台相机插件管理的临时前端配置；不是内容资产，不进入Cook或服务器。 */
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformCinematicCameraProfile> PreviewCameraProfile = nullptr;

    /** 预览镜头FOV快照，只在本舞台会话内保存并在退出过渡完成后恢复。 */
    TWeakObjectPtr<AGamePlatformCharacterPreviewStage> CameraFovOwnerStage;
    float CameraFieldOfViewBeforePreview = 90.0f;
    bool bHasCameraFieldOfViewSnapshot = false;

    /** 退出过渡期间保留舞台与镜头源；重新进入可取消延迟卸载。 */
    FTimerHandle DeferredPreviewTeardownHandle;
    bool bPreviewDeactivationPending = false;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DevelopmentDynamicMaterials;

    /** 缺配置默认桃花花瓣，允许蓝图选择Peach/Maple/Bamboo/Ginkgo。 */
    FName PreviewFoliageStyle = TEXT("Peach");
    FName RequestedHeroDefinitionId = NAME_None;
    int32 RequestGeneration = 0;
    TSharedPtr<FStreamableHandle> ProfileLease;
    TSharedPtr<FStreamableHandle> VisualLease;
    FDelegateHandle WorldCleanupHandle;
};
