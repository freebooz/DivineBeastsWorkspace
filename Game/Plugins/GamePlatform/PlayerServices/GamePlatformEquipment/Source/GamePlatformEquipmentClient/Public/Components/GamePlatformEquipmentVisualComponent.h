// Actor作用域复制/视觉组件；游戏线程事件驱动，组件退出取消自己的加载与委托；不修改后端物品真源。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GamePlatformEquipmentVisualComponent.generated.h"

class UGamePlatformEquipmentComponent;
class UGamePlatformEquipmentVisualDefinition;
struct FStreamableHandle;
class USkeletalMeshComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMEQUIPMENTCLIENT_API UGamePlatformEquipmentVisualComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamePlatformEquipmentVisualComponent();

    /** 游戏线程登记已加载的有效外观定义；组件反射引用保活，不转移数据服务资产所有权。 */
    void RegisterVisualDefinition(
        UGamePlatformEquipmentVisualDefinition* Definition);

    /** 游戏线程换绑复制状态组件；解除旧组件事件、读取新公开投影并重建，nullptr清空当前视觉。 */
    void BindEquipmentComponent(
        UGamePlatformEquipmentComponent* InEquipmentComponent);

    /** 游戏线程换绑Avatar网格及非负代次；旧异步视觉完成失效，仅创建附着视觉，不创建权威碰撞。 */
    void BindAvatar(
        USkeletalMeshComponent* InAvatarMesh,
        int32 InAvatarGeneration);

    /** 游戏线程整份预检公开槽位唯一性后更新；失败保留已有可清理对象，缺资源仅影响视觉，退出取消自身句柄。 */
    void RefreshVisuals();

protected:
    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

private:
    TWeakObjectPtr<UGamePlatformEquipmentComponent> EquipmentComponent;
    TWeakObjectPtr<USkeletalMeshComponent> AvatarMesh;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGamePlatformEquipmentVisualDefinition>>
        Definitions;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UStaticMeshComponent>>
        ActiveStaticMeshes;

    TMap<FName, TSharedPtr<FStreamableHandle>>
        ActiveLoadHandles;

    int32 AvatarGeneration = 0;
    uint64 VisualRequestGeneration = 0;

    void ClearVisuals();

    void RequestStaticMesh(
        FName SlotId,
        UGamePlatformEquipmentVisualDefinition* Definition,
        uint64 ExpectedRequestGeneration,
        int32 ExpectedAvatarGeneration);
};
