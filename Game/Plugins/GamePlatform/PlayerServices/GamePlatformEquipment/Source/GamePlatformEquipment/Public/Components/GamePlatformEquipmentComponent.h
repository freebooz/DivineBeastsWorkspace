// Actor作用域复制/视觉组件；游戏线程事件驱动，组件退出取消自己的加载与委托；不修改后端物品真源。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/GamePlatformEquipmentTypes.h"
#include "GamePlatformEquipmentComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FGamePlatformEquipmentChanged);

UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMEQUIPMENT_API UGamePlatformEquipmentComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamePlatformEquipmentComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 游戏线程只读借用拥有者私有投影；服务端复制COND_OwnerOnly，不包含GAS句柄，下一变化/退出后引用失效。 */
    const FGamePlatformEquipmentSnapshot& GetOwnerSnapshot() const
    {
        return OwnerSnapshot;
    }

    /** 游戏线程只读借用公开装备外观身份；不泄露物品实例与库存，不承担授权。 */
    const FGamePlatformPublicEquipmentSnapshot& GetPublicSnapshot() const
    {
        return PublicSnapshot;
    }

    UFUNCTION(BlueprintPure, Category="Equipment")
    int32 GetEquipmentRuntimeGeneration() const
    {
        return EquipmentRuntimeGeneration;
    }

    /** 仅服务器游戏线程：完整快照预检失败整份拒绝；成功同步拥有者/公开投影并广播，运行代次非负，不自行落库或授予GAS。 */
    void SetServerSnapshots(
        const FGamePlatformEquipmentSnapshot& InOwnerSnapshot,
        int32 InRuntimeGeneration);

    FGamePlatformEquipmentChanged OnOwnerEquipmentChanged;
    FGamePlatformEquipmentChanged OnPublicEquipmentChanged;

private:
    UPROPERTY(ReplicatedUsing=OnRep_OwnerSnapshot)
    FGamePlatformEquipmentSnapshot OwnerSnapshot;

    UPROPERTY(ReplicatedUsing=OnRep_PublicSnapshot)
    FGamePlatformPublicEquipmentSnapshot PublicSnapshot;

    UPROPERTY(Replicated)
    int32 EquipmentRuntimeGeneration = 0;

    UFUNCTION()
    void OnRep_OwnerSnapshot();

    UFUNCTION()
    void OnRep_PublicSnapshot();

    static FGamePlatformPublicEquipmentSnapshot BuildPublicSnapshot(
        const FGamePlatformEquipmentSnapshot& Source);
};
