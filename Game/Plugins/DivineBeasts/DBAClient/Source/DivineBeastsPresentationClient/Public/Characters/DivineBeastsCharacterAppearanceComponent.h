#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "DivineBeastsCharacterAppearanceComponent.generated.h"

struct FStreamableHandle;
class UDivineBeastsCharacterComponent;
class UDivineBeastsCharacterAppearanceProfile;
class UMaterialInstanceDynamic;

/**
 * UDivineBeastsCharacterAppearanceComponent（神兽联盟客户端角色外观装配组件）。
 *
 * 只负责客户端表现：
 * - 读取项目角色组件已经复制的稳定HeroDefinitionId；
 * - 异步加载对应Appearance Profile及其Mesh/材质/动画；
 * - 把表现资源应用到ACharacter的Mesh组件；
 * - 身份切换或Actor销毁时取消旧异步请求，禁止旧资源回调污染新角色。
 *
 * 不参与服务器权威、碰撞、移动、技能、伤害或资格判断。
 */
UCLASS(ClassGroup=(DivineBeasts), Transient)
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsCharacterAppearanceComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UDivineBeastsCharacterAppearanceComponent();
    /** 本地受控Avatar的表现提示来自已验证选择；不写服务器英雄身份、技能或网络状态。 */
    void ApplyApprovedVisualHero(FName HeroId);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 原生回归只访问异步完成边界，验证引擎网络平滑所需的实际Mesh基准，不开放玩法API。
    friend class FDivineBeastsNetworkMeshPlacementTest;
    FName ApprovedVisualHero=NAME_None;
    FName CurrentVisualHero() const;
    /** 尝试绑定项目角色状态组件；动态复制组件尚未到达时使用有限次数定时重试，不启用Tick。 */
    void TryBindCharacterState();

    /** 角色Ready或身份事实变化后重新评估表现。 */
    void HandleCharacterReadinessChanged(bool bReady);

    /** 按当前HeroDefinitionId重新申请Appearance Profile。 */
    void RefreshAppearance();

    /** 游戏线程按已装配Profile和实际胶囊尺寸更新表现位置及引擎平滑基准；不修改权威胶囊、位置或资源租约。 */
    void RefreshMeshPlacement();

    void HandleProfileLoaded(
        UDivineBeastsCharacterAppearanceProfile* Profile,
        FName ExpectedHeroDefinitionId,
        int32 ExpectedRequestGeneration);

    void HandleVisualResourcesLoaded(
        UDivineBeastsCharacterAppearanceProfile* Profile,
        FName ExpectedHeroDefinitionId,
        int32 ExpectedRequestGeneration);

    /** 取消Profile与视觉资源的旧租约，并使已排队回调失效。 */
    void CancelPendingLoads();

    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsCharacterComponent> CharacterState = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsCharacterAppearanceProfile> PendingProfile = nullptr;

    /** 当前占位角色运行时动态材质；由Mesh组件和本数组共同持有，身份切换时整体替换。 */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> DevelopmentDynamicMaterials;

    FDelegateHandle ReadinessDelegateHandle;
    /** 与Readiness独立的公开Hero复制事件；EndPlay必须解绑，避免旧世界访问已释放组件。 */
    FDelegateHandle IdentityDelegateHandle;
    FTimerHandle StateRetryTimer;

    TSharedPtr<FStreamableHandle> ProfileLease;
    TSharedPtr<FStreamableHandle> VisualLease;

    FName AppliedHeroDefinitionId = NAME_None;
    int32 RequestGeneration = 0;
    int32 RemainingStateBindRetries = 20;
};
