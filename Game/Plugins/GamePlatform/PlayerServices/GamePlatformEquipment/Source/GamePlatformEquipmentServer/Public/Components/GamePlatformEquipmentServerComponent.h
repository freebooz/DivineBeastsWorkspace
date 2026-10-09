#pragma once

// 平台服务器装备运行组件：项目服务器注入已授权玩家/角色及持久化Port。
// 仅游戏线程；拥有自身GAS授予，弱引用ASC/复制组件；退出撤销授予并失效所有在飞回调。

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/GamePlatformEquipmentGASGrantPort.h"
#include "Types/GamePlatformEquipmentTypes.h"
#include "GamePlatformEquipmentServerComponent.generated.h"

class IGamePlatformEquipmentPersistencePort;
class UAbilitySystemComponent;
class UGamePlatformEquipmentComponent;
class UGamePlatformEquipmentDefinition;

UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMEQUIPMENTSERVER_API UGamePlatformEquipmentServerComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamePlatformEquipmentServerComponent();
    /** 世界/Actor退出：幂等撤销本组件授予，不删除持久化装备，也不清除其他能力。 */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    /** 移除组件但ASC仍存活时亦清理；重复清理无副作用。 */
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

    /** 初始化权威运行；玩家/角色归属须由调用方验证；false表示未受理，无生产替身；成功后由Port游戏线程回调加载。 */
    bool InitializeEquipmentRuntime(
        const FString& InPlayerId,
        const FString& InCharacterId,
        UGamePlatformEquipmentComponent* InStateComponent,
        TSharedPtr<IGamePlatformEquipmentPersistencePort, ESPMode::ThreadSafe> InPersistence,
        TSharedPtr<FGamePlatformEquipmentGASGrantPort, ESPMode::ThreadSafe> InGrantPort);

    /** 登记调用方已加载的合法定义；组件反射引用保活，非法定义不登记，生命周期内保持资产身份不变。 */
    void RegisterDefinition(
        UGamePlatformEquipmentDefinition* Definition);

    /** 绑定权威ASC与非负Avatar代次；切换先撤销旧授予；false表示新运行快照未能应用，不能宣布就绪。 */
    bool BindAvatar(
        UAbilitySystemComponent* InAbilitySystem,
        int32 InAvatarGeneration);

    /** 请求装备：须已就绪、OperationId有效且版本匹配；返回None仅表示异步受理，持久化成功后才授予。 */
    EGamePlatformEquipmentError RequestEquip(
        const FGamePlatformEquipRequest& Request);

    /** 请求卸下：版本冲突明确返回，成功落库后撤销本组件拥有的资源；取消不回滚已提交后端事务。 */
    EGamePlatformEquipmentError RequestUnequip(
        const FGamePlatformUnequipRequest& Request);

    /** 重新读取权威快照；false表示无可用Port或忙，失败保留错误，不用客户端快照替代。 */
    bool Reconcile();

    UFUNCTION(BlueprintPure, Category="Equipment")
    bool IsEquipmentGameplayReady() const
    {
        return bReady && !bRuntimeError;
    }

    UFUNCTION(BlueprintPure, Category="Equipment")
    EGamePlatformEquipmentError GetLastError() const
    {
        return LastError;
    }

private:
    FString PlayerId;
    FString CharacterId;

    TWeakObjectPtr<UGamePlatformEquipmentComponent> StateComponent;
    TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;

    TSharedPtr<IGamePlatformEquipmentPersistencePort, ESPMode::ThreadSafe>
        Persistence;
    TSharedPtr<FGamePlatformEquipmentGASGrantPort, ESPMode::ThreadSafe>
        GrantPort;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGamePlatformEquipmentDefinition>>
        Definitions;

    FGamePlatformEquipmentSnapshot Snapshot;
    TMap<FName, FGamePlatformEquipmentGameplayGrantHandle> SlotGrantHandles;

    /** 运行组件生命周期代次；销毁/重初始化使旧Port回调失效。 */
    uint64 LifetimeGeneration = 0;
    bool bRuntimeClosed = false;
    int32 AvatarGeneration = 0;
    int32 EquipmentRuntimeGeneration = 0;
    bool bReady = false;
    bool bRuntimeError = false;
    bool bPersistenceInFlight = false;
    EGamePlatformEquipmentError LastError = EGamePlatformEquipmentError::None;

    void HandleLoadCompleted(
        FGamePlatformEquipmentSnapshot Loaded,
        EGamePlatformEquipmentError Error);

    void HandleMutationCompleted(
        FGuid OperationId,
        FGamePlatformEquipmentSnapshot Persisted,
        EGamePlatformEquipmentError Error);

    bool ApplyRuntimeSnapshot(
        const FGamePlatformEquipmentSnapshot& Persisted);

    /** 停止接纳、失效回调、撤销自有授予，再释放Port与弱对象；不取消不可回滚的后端事务。 */
    void ShutdownEquipmentRuntime();
    void RevokeAllRuntimeGrants();
    void PublishSnapshot();
};
