#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "DivineBeastsCharacterPreviewSubsystem.generated.h"

class AGamePlatformCharacterPreviewStage;
class ADefaultPawn;
class UDivineBeastsCharacterAppearanceProfile;
class ULevelStreamingDynamic;
class UMaterialInstanceDynamic;
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

    void ResolvePreviewStage();
    void BindPreviewCamera();
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

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> PreviousViewTarget;

    /** 仅借用当前本地默认观测Pawn的可见性，避免球体遮挡脚部；退出时恢复原值。 */
    UPROPERTY(Transient)
    TWeakObjectPtr<ADefaultPawn> HiddenPreviewObserver;
    bool bObserverWasHidden = false;

    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsCharacterAppearanceProfile> PendingProfile = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DevelopmentDynamicMaterials;

    FName RequestedHeroDefinitionId = NAME_None;
    int32 RequestGeneration = 0;
    TSharedPtr<FStreamableHandle> ProfileLease;
    TSharedPtr<FStreamableHandle> VisualLease;
    FDelegateHandle WorldCleanupHandle;
};
