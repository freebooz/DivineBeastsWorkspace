// 项目客户端石灵视觉装配；只读取复制生命投影，拥有本世界装饰Actor/租约，绝不写服务器生命。
#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "Types/GamePlatformDataLease.h"
#include "TimerManager.h"
#include "DivineBeastsDevelopmentMonsterVisualWorldSubsystem.generated.h"
class ADivineBeastsDevelopmentMonster;
struct FDivineBeastsDevelopmentMonsterLifeState;

/** 私有开发视觉服务，无UI业务轮询、无怪物AI和伤害；Dedicated Server不包含该模块。 */
UCLASS()
class UDivineBeastsDevelopmentMonsterVisualWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;
private:
    /** 仅原生回归夹具可进入历史通知/资源就绪边界；不扩展运行时公开API，不伪造Data租约。 */
    friend struct FDivineBeastsDevelopmentMonsterVisualTestFixture;
    /** 每怪物独占装饰与受击计时器，复制Actor不被客户端替换或销毁。 */
    struct FVisual
    {
        FDelegateHandle LifeHandle;
        TWeakObjectPtr<AActor> Actor;
        FTimerHandle HitTimer;
        float PreviousHealth=0;
        int32 Generation=0;
        /** 每次观察的独立身份；同一Monster移除后重建的记录不能被旧Spawn/Timer操作。 */
        FGuid ObservationId;
        /** 每次显示更新的独立身份；同步后继生命更新立即使旧栈和旧受击Timer失效。 */
        FGuid UpdateId;
        /** Spawn/附着期间阻止同步生命通知二次生成；创建完成后重新采样当前生命。 */
        bool bCreatingActor=false;
        /** 本观察首次真实Actor和两个UIFont标签装配成功才记录，不随受伤/重生重复刷日志。 */
        bool bVisualReadyLogged=false;
    };
    TMap<TWeakObjectPtr<ADivineBeastsDevelopmentMonster>,FVisual> Visuals;
    FGamePlatformDataLease Lease;
    FDelegateHandle SpawnHandle;
    FGuid LifetimeToken;
    bool bReady=false, bClosing=false;
    void ObserveActor(AActor* Actor);
    /** 游戏线程生命事件更新文案/受击/死亡；迟到加载仅读取当前快照，不补播旧受击。 */
    void ApplyLife(ADivineBeastsDevelopmentMonster* Monster,const FDivineBeastsDevelopmentMonsterLifeState& State);
    /** 只读GT核查世界及观察身份；返回后不能跨外部调用持有Map元素指针。 */
    bool IsObservationCurrent(ADivineBeastsDevelopmentMonster* Monster,UWorld* World,
        const FGuid& WorldToken,const FGuid& ObservationId) const;
    /** 同时核对装饰、显示操作及当前完整生命，拒绝同代次旧Health/死亡值与同步后继更新的旧栈。 */
    bool IsVisualOperationCurrent(ADivineBeastsDevelopmentMonster* Monster,UWorld* World,
        const FGuid& WorldToken,const FGuid& ObservationId,const FGuid& UpdateId,AActor* Actor,
        const FDivineBeastsDevelopmentMonsterLifeState& State) const;
    /** 只释放自己的装饰、事件和计时器；真实Monster归服务器/网络生命周期所有。 */
    void RemoveVisual(ADivineBeastsDevelopmentMonster* Monster);
    UFUNCTION() void HandleActorEndPlay(AActor* Actor,EEndPlayReason::Type Reason);
};
