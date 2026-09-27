#pragma once
#include "Components/ActorComponent.h"
#include "Interfaces/IGamePlatformGameplayService.h"
#include "Interfaces/IGamePlatformExperienceAssembly.h"
#include "Types/GamePlatformDataLease.h"
#include "GamePlatformExperienceComponent.generated.h"

class UGamePlatformExperienceDefinition;
class UGamePlatformPawnDefinition;
struct FGamePlatformExperienceRuntime;

/** GameState唯一体验所有者；复制状态和本端Data租约同源，内部注册器不向调用者暴露。 */
UCLASS(ClassGroup=(GamePlatform), NotBlueprintable)
class GAMEPLATFORMGAMEPLAY_API UGamePlatformExperienceComponent : public UActorComponent, public IGamePlatformGameplayService
{
    GENERATED_BODY()
public:
    UGamePlatformExperienceComponent();
    UGamePlatformExperienceComponent(FVTableHelper& Helper);
    virtual ~UGamePlatformExperienceComponent() override;
    /** 服务器选择合法资产ID；非Development授权不能选开发定义。仅Unassigned可受理，热切换返回Unsupported。 */
    FGamePlatformResult BeginExperience(const FPrimaryAssetId& DefinitionId, bool bAllowDevelopmentDefinition = false);
    /** 服务器停止新操作，先清理玩家，再逆序释放装配与租约。重复调用成功。 */
    FGamePlatformResult BeginExperienceDrain(FName Reason = NAME_None);
    /** 启动前注册唯一工厂；弱Owner须本世界，最多64；句柄由本组件校验。 */
    FGamePlatformGameplayRegistration RegisterAssemblyFactory(FName FactoryId, TWeakObjectPtr<UObject> Owner,
        FGamePlatformAssemblyFactory Factory, FGamePlatformResult& OutResult);
    /** 活动项对应工厂撤销时体验失败并排空，不能留下半套规则。 */
    bool UnregisterAssemblyFactory(const FGamePlatformGameplayRegistration& Registration);
    virtual FGamePlatformExperienceSnapshot GetExperienceSnapshot() const override { return Snapshot; }
    virtual FGamePlatformClientExperienceSnapshot GetClientExperienceSnapshot() const override;
    virtual FGamePlatformPlayerLifecycleSnapshot GetPlayerLifecycleSnapshot(const APlayerState& Player) const override;
    virtual FGamePlatformGameplayRegistration SubscribeExperienceChanged(TWeakObjectPtr<UObject> Owner,
        TFunction<void(const FGamePlatformExperienceSnapshot&)> Callback, FGamePlatformResult& OutResult) override;
    virtual bool Unsubscribe(const FGamePlatformGameplayRegistration& Registration) override;
    virtual FGamePlatformGameplayDiagnostics GetDiagnostics() const override;
    /** 游戏线程借用只读定义，仅当前组件租约仍有效时非空；禁止跨帧或清理后缓存裸指针。 */
    const UGamePlatformExperienceDefinition* GetLoadedExperience() const;
    /** 返回当前根体验配置的Pawn定义；每玩家另持租约。 */
    const UGamePlatformPawnDefinition* GetLoadedDefaultPawn() const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_Snapshot)
    FGamePlatformExperienceSnapshot Snapshot;
    UFUNCTION() void OnRep_Snapshot();
    TUniquePtr<FGamePlatformExperienceRuntime> Runtime;
    void SetStage(EGamePlatformExperienceStage Stage, FName Error = NAME_None);
    void StartLocalLoad(const FPrimaryAssetId& Id);
    void AdvancePreparation();
    void Fail(FName Error);
    void ReleaseResources();
    void DispatchSnapshot();
};
