// SSLM客户端狼外观装配：统一Data租约加载纯美术，服务器绝不Cook狼网格/动画/纹理。
#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformDataLease.h"
#include "DivineBeastsVillageWolfVisualWorldSubsystem.generated.h"

class ADivineBeastsVillageWolf;

/**
 * 只维护本地可见的狼SkeletalMesh/AnimBP与平台Data世界租约。
 * 不生成怪物、不写入血量、不执行客户端战斗与重复资源管理器。
 */
UCLASS()
class UDivineBeastsVillageWolfVisualWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

private:
    /** 观察的复制狼Actor弱集合，避免跨世界持有对象。 */
    TSet<TWeakObjectPtr<ADivineBeastsVillageWolf>> ObservedWolves;
    FDelegateHandle SpawnHandle;
    FGamePlatformDataLease VisualLease;
    FSoftObjectPath WolfMeshPath;
    FSoftObjectPath WolfAnimPath;
    /** 经实际模型边界校准的纯客户端导入比例/朝向，不修改权威胶囊或导航代理。 */
    float WolfMeshScale = 1.0f;
    float WolfMeshYawDegrees = 0.0f;
    FGuid Operation;
    bool bReady = false;
    bool bLoading = false;
    bool bClosing = false;

    /** 只识别真实复制Actor；该调用可能发生在BeginPlay或迟到生成事件中。 */
    void ObserveActor(AActor* Spawned);
    /** 唯一GamePlatformData加载，不允许直接StreamableManager旁路。 */
    void BeginLoadVisuals();
    /** 租约及世界代次核对后向本地Mesh组件应用外观。 */
    void ApplyVisual(ADivineBeastsVillageWolf& Wolf);
    /** Actor退出时只删除观察，不Destroy复制Actor或卸载其他人的租约。 */
    UFUNCTION() void HandleWolfEndPlay(AActor* Ended, EEndPlayReason::Type Reason);
};
